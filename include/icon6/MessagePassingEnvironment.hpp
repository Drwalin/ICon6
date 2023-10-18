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

#include "Util.hpp"
#include "Host.hpp"
#include "Peer.hpp"
#include "OnReturnCallback.hpp"
#include "MessageConverter.hpp"

namespace icon6
{

class CommandExecutionQueue;

class MessagePassingEnvironment
	: public std::enable_shared_from_this<MessagePassingEnvironment>
{
public:
	virtual void OnReceive(Peer *peer, ByteReader &reader, Flags flags);

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

	void CheckForTimeoutFunctionCalls(uint32_t maxChecks = 10);

protected:
	std::unordered_map<std::string, std::shared_ptr<MessageConverter>>
		registeredMessages;

	std::mutex mutexReturningCallbacks;
	uint32_t returnCallCallbackIdGenerator;
	std::unordered_map<uint32_t, OnReturnCallback> returningCallbacks;
	uint32_t lastCheckedId;
};
} // namespace icon6

#endif
