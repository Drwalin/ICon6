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

#include <chrono>

#include "../include/icon6/ByteReader.hpp"
#include "../include/icon6/CommandExecutionQueue.hpp"
#include "../include/icon6/MessagePassingEnvironment.hpp"

namespace icon6
{
MessagePassingEnvironment::~MessagePassingEnvironment()
{
	for (auto it : registeredMessages) {
		delete it.second;
	}
	registeredMessages.clear();
}

void MessagePassingEnvironment::OnReceive(Peer *peer, ByteReader &reader,
										  Flags flags)
{
	uint8_t methodInvokeType;
	reader.op(methodInvokeType);
	if (methodInvokeType == MethodProtocolSendFlags::RETURN_CALLBACK) {
		uint32_t id;
		reader.op(id);
		OnReturnCallback callback;
		bool found = false;
		{
			std::lock_guard guard{mutexReturningCallbacks};
			auto it2 = returningCallbacks.find(id);
			if (it2 != returningCallbacks.end()) {
				callback = std::move(it2->second);
				returningCallbacks.erase(it2);
				found = true;
			}
		}
		if (found) {
			callback.Execute(peer, flags, reader);
		} else {
			// TODO: returned valued didn't found callback
		}
	} else {
		std::string name;
		reader.op(name);
		auto it = registeredMessages.find(name);
		if (registeredMessages.end() != it) {
			auto mtd = it->second;
			if (mtd->executionQueue) {
				Command command{commands::ExecuteRPC(std::move(reader))};
				commands::ExecuteRPC &com = command.executeRPC;
				com.peer = peer;
				com.flags = flags;
				com.messageConverter = mtd;
				mtd->executionQueue->EnqueueCommand(std::move(command));
			} else {
				mtd->Call(peer, reader, flags);
			}
		}
	}
}

void MessagePassingEnvironment::CheckForTimeoutFunctionCalls(uint32_t maxChecks)
{
	std::vector<OnReturnCallback> timeouts;
	auto now = std::chrono::steady_clock::now();
	if (mutexReturningCallbacks.try_lock()) {
		auto it = returningCallbacks.find(lastCheckedId);
		if (it == returningCallbacks.end())
			it = returningCallbacks.begin();
		for (int i = 0; i < maxChecks && it != returningCallbacks.end(); ++i) {
			lastCheckedId = it->first;
			if (it->second.IsExpired(now)) {
				auto next = it;
				++next;
				uint32_t nextId = 0;
				if (next != returningCallbacks.end()) {
					nextId = next->first;
				}
				timeouts.push_back(std::move(it->second));
				returningCallbacks.erase(lastCheckedId);
				it = returningCallbacks.find(nextId);
			} else {
				++it;
			}
		}
		mutexReturningCallbacks.unlock();
	}
	for (auto &t : timeouts) {
		t.ExecuteTimeout();
	}
}
} // namespace icon6
