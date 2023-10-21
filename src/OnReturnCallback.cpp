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

#include "../include/icon6/OnReturnCallback.hpp"

namespace icon6
{
bool OnReturnCallback::IsExpired(
	std::chrono::time_point<std::chrono::steady_clock> t) const
{
	return t > timeoutTimePoint;
}

void OnReturnCallback::Execute(Peer *peer, Flags flags, ByteReader &reader)
{
	if (peer != this->peer) {
		// TODO: implement check
		throw "Received return value from different peer than request was "
			  "sent to.";
	}

	if (executionQueue) {
		Command command{commands::ExecuteReturnRC{std::move(reader)}};
		commands::ExecuteReturnRC &com = command.executeReturnRC;
		com.peer = this->peer;
		com.function = onReturnedValue;
		com.flags = flags;
		executionQueue->EnqueueCommand(std::move(command));
	} else {
		onReturnedValue(this->peer, flags, reader);
	}
}

void OnReturnCallback::ExecuteTimeout()
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
} // namespace icon6
