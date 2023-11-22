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

#include <cstring>

#include <thread>

#include "../include/icon6/MessagePassingEnvironment.hpp"
#include "../include/icon6/Host.hpp"
#include "../include/icon6/Command.hpp"

#include "../include/icon6/Peer.hpp"

namespace icon6
{
Peer::Peer(Host *host)
	: host(host)
{
	userData = 0;
	userPointer = nullptr;
	callbackOnReceive = host->callbackOnReceive;
	callbackOnDisconnect = host->callbackOnDisconnect;
	readyToUse = false;
}
Peer::~Peer() {}

void Peer::Send(std::vector<uint8_t> &&data, Flags flags)
{
	if (!IsReadyToUse()) {
		DEBUG("Send called on peer %p before ready to use", this);
	}
	Command command{commands::ExecuteSend{}};
	commands::ExecuteSend &com = std::get<commands::ExecuteSend>(command.cmd);
	com.flags = flags;
	com.peer = this;
	com.data = std::move(data);
	host->EnqueueCommand(std::move(command));
}

void Peer::_InternalSendOrQueue(std::vector<uint8_t> &data, Flags flags)
{
	bool wasQueued = !queuedSends.empty();
	_InternalFlushQueuedSends();
	if (queuedSends.empty() ||
		((flags & FLAG_UNRELIABLE_NO_NAGLE) == FLAG_UNRELIABLE_NO_NAGLE)) {
		if (_InternalSend(data.data(), data.size(), flags) == true) {
			if (wasQueued) {
				host->peersQueuedSends.erase(this);
				host->peersQueuedSendsIterator = host->peersQueuedSends.begin();
			}
			return;
		}
	}
	queuedSends.emplace();
	queuedSends.back().data.swap(data);
	queuedSends.back().flags = flags;
	host->peersQueuedSends.insert(this);
	host->peersQueuedSendsIterator = host->peersQueuedSends.begin();
}

void Peer::_InternalFlushQueuedSends()
{
	while (queuedSends.empty() == false) {
		SendCommand &cmd = queuedSends.front();
		if (_InternalSend(cmd.data.data(), cmd.data.size(), cmd.flags)) {
			queuedSends.pop();
		} else {
			break;
		}
	}
}

void Peer::Disconnect()
{
	Command command{commands::ExecuteDisconnect{}};
	commands::ExecuteDisconnect &com =
		std::get<commands::ExecuteDisconnect>(command.cmd);
	com.peer = this;
	host->EnqueueCommand(std::move(command));
}

void Peer::SetReceiveCallback(void (*callback)(Peer *, ByteReader &, Flags))
{
	callbackOnReceive = callback;
}

void Peer::SetDisconnect(void (*callback)(Peer *))
{
	callbackOnDisconnect = callback;
}

void Peer::SetReadyToUse() { readyToUse = true; }
} // namespace icon6
