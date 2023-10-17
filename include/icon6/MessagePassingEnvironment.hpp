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

#ifndef ICON6_MESSAGE_PASSING_ENVIRONMENT_HPP
#define ICON6_MESSAGE_PASSING_ENVIRONMENT_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>

#include <bitscpp/ByteWriterExtensions.hpp>

#include "Host.hpp"
#include "Peer.hpp"
#include "MessageConverter.hpp"

namespace icon6
{

class CommandExecutionQueue;

template <typename... Args>
auto ConvertLambdaToFunctionPtr(void (*fun)(Args...))
{
	return fun;
}

template <typename Fun> auto ConvertLambdaToFunctionPtr(Fun &&fun)
{
	return +fun;
}

template <typename T> auto MakeShared(T *ptr)
{
	return std::shared_ptr<T>(ptr);
}

class OnReturnCallback
{
public:
	~OnReturnCallback() = default;

	bool IsExpired() const
	{
		return std::chrono::steady_clock::now() < timeoutTimePoint;
	}

	void Execute(Peer *peer, Flags flags, std::vector<uint8_t> &bytes,
				 uint32_t offset)
	{
		if (peer != this->peer.get()) {
			// TODO: implement check
			throw "Received return value from different peer than request was "
				  "sent to.";
		}

		if (executionQueue) {
			Command command{commands::ExecuteReturnRC{}};
			commands::ExecuteReturnRC &com = command.executeReturnRC;
			com.peer = this->peer;
			com.function = onReturnedValue;
			com.flags = flags;
			com.readOffset = offset;
			std::swap(com.binaryData, bytes);
			executionQueue->EnqueueCommand(std::move(command));
		} else {
			onReturnedValue(this->peer, flags, bytes, offset);
		}
	}

	void ExecuteTimeout()
	{
		if (onTimeout) {
			if (executionQueue) {
				Command command{commands::ExecuteFunctionObjectNoArgsOnPeer{}};
				commands::ExecuteFunctionObjectNoArgsOnPeer &com =
					command.executeFunctionObjectNoArgsOnPeer;
				com.peer = peer;
				com.function = onTimeout;
				executionQueue->EnqueueCommand(std::move(command));
			} else {
				onTimeout(peer);
			}
		}
	}

public:
	std::function<void(std::shared_ptr<Peer>, Flags, std::vector<uint8_t> &,
					   uint32_t)>
		onReturnedValue;
	std::function<void(std::shared_ptr<Peer>)> onTimeout;
	std::shared_ptr<CommandExecutionQueue> executionQueue;
	std::chrono::time_point<std::chrono::steady_clock> timeoutTimePoint;
	std::shared_ptr<Peer> peer;
};

template <typename Tret>
OnReturnCallback MakeOnReturnCallback(
	std::function<void(std::shared_ptr<Peer>, Flags, Tret)> _onReturnedValue,
	std::function<void(std::shared_ptr<Peer>)> onTimeout,
	uint32_t timeoutMilliseconds, std::shared_ptr<Peer> peer,
	std::shared_ptr<CommandExecutionQueue> executionQueue = nullptr)
{
	OnReturnCallback ret;
	OnReturnCallback *self = &ret;

	self->onTimeout = onTimeout;
	self->executionQueue = executionQueue;
	self->timeoutTimePoint = std::chrono::steady_clock::now() +
							 (std::chrono::milliseconds(timeoutMilliseconds));
	self->peer = peer;
	self->onReturnedValue = [_onReturnedValue](std::shared_ptr<Peer> peer,
											   Flags flags,
											   std::vector<uint8_t> &bytes,
											   uint32_t offset) -> void {
		typename std::remove_const<
			typename std::remove_reference<Tret>::type>::type ret;
		bitscpp::ByteReader<false> reader(bytes.data() + offset,
										  bytes.size() - offset);
		reader.op(ret);
		_onReturnedValue(peer, flags, ret);
	};

	return ret;
}

class MessagePassingEnvironment
	: public std::enable_shared_from_this<MessagePassingEnvironment>
{
public:
	virtual void OnReceive(Peer *peer, std::vector<uint8_t> &data, Flags flags);

	template <typename Fun>
	void RegisterMessage(
		const std::string &name, Fun &&fun,
		std::shared_ptr<CommandExecutionQueue> executionQueue = nullptr)
	{
		auto f = ConvertLambdaToFunctionPtr(fun);
		auto func = MakeShared(new MessageConverterSpec(f));
		func->executionQueue = executionQueue;
		registeredMessages[name] = func;
	}

	template <typename... Targs>
	void Send(Peer *peer, Flags flags, const std::string &name,
			  const Targs &...args)
	{
		std::vector<uint8_t> buffer;
		{
			bitscpp::ByteWriter writer(buffer);
			writer.op(name);
			(writer.op(args), ...);
		}
		peer->Send(std::move(buffer), flags);
	}

	template <typename Tret, typename... Targs>
	void Call(Peer *peer, Flags flags, OnReturnCallback &&callback,
			  const std::string &name, const Targs &...args)
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
			writer.op((uint8_t)2);
			writer.op(name);
			(writer.op(args), ...);
			writer.op(returnCallbackId);
		}
		peer->Send(std::move(buffer), flags);
	}

protected:
	std::unordered_map<std::string, std::shared_ptr<MessageConverter>>
		registeredMessages;

	std::mutex mutexReturningCallbacks;
	uint32_t returnCallCallbackIdGenerator;
	std::unordered_map<uint32_t, OnReturnCallback> returningCallbacks;
};
} // namespace icon6

#endif
