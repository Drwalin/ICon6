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

#include "../include/icon6/CommandExecutionQueue.hpp"

#include "../include/icon6/MessagePassingEnvironment.hpp"

namespace icon6
{
void MessagePassingEnvironment::OnReceive(Peer *peer,
										  std::vector<uint8_t> &data,
										  Flags flags)
{
	bitscpp::ByteReader reader(data.data(), data.size());
	std::string name;
	if (data[0] == 2) {
		uint8_t b;
		reader.op(b);
	}
	reader.op(name);
	auto it = registeredMessages.find(name);
	if (registeredMessages.end() != it) {
		auto mtd = it->second;
		if (mtd->executionQueue) {
			Command command{commands::ExecuteRPC{}};
			commands::ExecuteRPC &com = command.executeRPC;
			com.peer = peer->shared_from_this();
			com.flags = flags;
			com.binaryData.swap(data);
			com.readOffset = reader.get_offset();
			com.messageConverter = mtd;
			mtd->executionQueue->EnqueueCommand(std::move(command));
		} else {
			mtd->Call(peer, reader, flags);
		}
	} else if (name == std::string_view("_ret")) {
		uint32_t id;
		reader.op(id);
		OnReturnCallback callback;
		{
			std::lock_guard guard{mutexReturningCallbacks};
			auto it2 = returningCallbacks.find(id);
			if (it2 != returningCallbacks.end()) {
				callback = std::move(it2->second);
				returningCallbacks.erase(it2);
			}
		}
		callback.Execute(peer, flags, data, reader.get_offset());
	}
}
} // namespace icon6
