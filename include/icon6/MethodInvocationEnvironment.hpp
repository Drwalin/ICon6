/*
 *  This file is part of ICon6.
 *  Copyright (C) 2023 Marek Zalewski aka Drwalin
 *
 *  ICon6 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  ICon6 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef ICON6_METHOD_INVOCATION_ENVIRONMENT_HPP
#define ICON6_METHOD_INVOCATION_ENVIRONMENT_HPP

#include <memory>

#include "MethodInvocationConverter.hpp"
#include "Flags.hpp"

#include "MessagePassingEnvironment.hpp"

namespace icon6
{
namespace rmi
{

class Class
{
public:
	Class(Class *parentClass, std::string name,
		  std::shared_ptr<void> (*constructor)());

	void RegisterMethod(std::string methodName,
						std::shared_ptr<MethodInvocationConverter> converter);

	std::unordered_map<std::string, std::shared_ptr<MethodInvocationConverter>>
		methods;
	std::shared_ptr<void> (*const constructor)();

	Class *const parentClass;
	std::unordered_set<Class *> inheritedClasses;
	const std::string name;
};

class Object
{
public:
	std::shared_ptr<void> objectPtr;
	Class *obejctClass;
};

class MethodInvocationEnvironment : public MessagePassingEnvironment
{
public:
	Class *GetClassByName(std::string name);

	virtual void OnReceive(Peer *peer, ByteReader &reader,
						   Flags flags) override;

	template <typename T>
	void RegisterClass(std::string className, Class *parentClass)
	{
		Class *cls =
			new Class(parentClass, className, []() -> std::shared_ptr<void> {
				return std::make_shared<T>();
			});
		classes[className] = cls;
	}

	template <typename Tclass, typename Fun>
	void RegisterMemberFunction(
		std::string className, std::string methodName, Fun &&memberFunction,
		std::shared_ptr<CommandExecutionQueue> executionQueue = nullptr)
	{
		Class *cls = GetClassByName(className);
		if (cls) {
			auto mtd =
				MakeShared(new MessageNetworkAwareMethodInvocationConverterSpec(
					cls, memberFunction));
			mtd->executionQueue = executionQueue;
			cls->RegisterMethod(methodName, mtd);
		} else {
			throw std::string("No class named '") + className + "' found.";
		}
	}

	template <typename T> std::shared_ptr<T> GetObject(uint64_t id)
	{
		auto it = objects.find(id);
		if (it != objects.end())
			return std::static_pointer_cast<T>(it->second.objectPtr);
		return nullptr;
	}

	template <typename T>
	std::shared_ptr<T> CreateLocalObject(std::string className, uint64_t &id)
	{
		static uint64_t ids = 1;
		id = ++ids;
		auto cls = GetClassByName(className);
		if (cls) {
			auto obj = cls->constructor();
			objects[id] = Object{obj, cls};
			return std::static_pointer_cast<T>(obj);
		}
		return nullptr;
	}

	template <typename... Targs>
	void SendInvoke(Peer *peer, Flags flags, uint64_t objectId,
					const std::string &name, const Targs &...args)
	{
		std::vector<uint8_t> buffer;
		{
			bitscpp::ByteWriter writer(buffer);
			writer.op(MethodProtocolSendFlags::METHOD_SEND_PREFIX);
			writer.op(objectId);
			writer.op(name);
			(writer.op(args), ...);
		}
		peer->Send(std::move(buffer), flags);
	}

	template <typename... Targs>
	void CallInvoke(Peer *peer, Flags flags, OnReturnCallback &&callback,
					uint64_t objectId, const std::string &name,
					const Targs &...args)
	{
		std::vector<uint8_t> buffer;
		uint32_t returnCallbackId = 0;
		{
			std::lock_guard guard{mutexReturningCallbacks};
			do {
				returnCallbackId = ++returnCallCallbackIdGenerator;
			} while (returnCallbackId == 0 &&
					 returningCallbacks.count(returnCallbackId) != 0);
			returningCallbacks[returnCallbackId] = std::move(callback);
		}
		{
			bitscpp::ByteWriter writer(buffer);
			writer.op(MethodProtocolSendFlags::METHOD_CALL_PREFIX);
			writer.op(objectId);
			writer.op(name);
			(writer.op(args), ...);
			writer.op(returnCallbackId);
		}
		peer->Send(std::move(buffer), flags);
	}

protected:
	std::unordered_map<size_t, Object> objects;
	std::unordered_map<std::string, Class *> classes;
};
} // namespace rmi
} // namespace icon6

#endif
