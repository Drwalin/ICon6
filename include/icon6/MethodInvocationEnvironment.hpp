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

#include "MessagePassingEnvironment.hpp"

namespace icon6
{
namespace rmi
{

class Class : public std::enable_shared_from_this<Class>
{
  public:
	Class(std::shared_ptr<Class> parentClass, std::string name,
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
	std::shared_ptr<Class> GetClassByName(std::string name);

	virtual void OnReceive(Peer *peer, std::vector<uint8_t> &data,
						   uint32_t flags) override;

	template <typename T>
	void RegisterClass(std::string className,
					   std::shared_ptr<Class> parentClass)
	{
		auto cls = std::make_shared<Class>(
			parentClass, className,
			[]() -> std::shared_ptr<void> { return std::make_shared<T>(); });
		classes[className] = cls;
	}

	template <typename Tclass, typename Targ>
	void RegisterMemberFunction(
		std::string className, std::string methodName,
		void (Tclass::*memberFunction)(Peer *, uint32_t flags, Targ data),
		std::shared_ptr<CommandExecutionQueue> executionQueue = nullptr)
	{
		std::shared_ptr<Class> cls = GetClassByName(className);
		if (cls) {
			auto mtd = std::make_shared<
				MessageNetworkAwareMethodInvocationConverterSpec<Tclass, Targ>>(
				cls.get(), memberFunction);
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
			objects[id] = Object{obj, cls.get()};
			return std::static_pointer_cast<T>(obj);
		}
		return nullptr;
	}

	template <typename T>
	void SendInvoke(Peer *peer, uint32_t flags, uint64_t objectId, const std::string &name,
					const T &message)
	{
		std::vector<uint8_t> buffer;
		{
			bitscpp::ByteWriter writer(buffer);
			writer.op((uint8_t)0);
			writer.op(objectId);
			writer.op(name);
			writer.op(message);
		}
		peer->Send(std::move(buffer), flags);
	}

  protected:
	std::unordered_map<size_t, Object> objects;
	std::unordered_map<std::string, std::shared_ptr<Class>> classes;
};
} // namespace rmi
} // namespace icon6

#endif
