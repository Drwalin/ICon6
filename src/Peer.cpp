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
SteamNetConnectionRealTimeStatus_t Peer::GetRealTimeStats()
{
	SteamNetConnectionRealTimeStatus_t status;
	host->host->GetConnectionRealTimeStatus(peer, &status, 0, nullptr);
	return status;
}

Peer::Peer(Host *host, HSteamNetConnection connection)
	: host(host), peer(connection)
{
	userData = 0;
	userPointer = nullptr;
	host->host->SetConnectionUserData(peer, (uint64_t)(size_t)this);
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
	commands::ExecuteSend &com = command.executeSend;
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

bool Peer::_InternalSend(const void *data, uint32_t length, const Flags flags)
{
	auto result = host->host->SendMessageToConnection(peer, data, length,
													  flags.field, nullptr);
	if (result == k_EResultLimitExceeded) {
		return false;
	}
	return true;
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
	commands::ExecuteDisconnect &com = command.executeDisconnect;
	com.peer = this;
	host->EnqueueCommand(std::move(command));
}

void Peer::_InternalDisconnect()
{
	if (peer) {
		host->host->CloseConnection(peer, 0, nullptr, true);
	}
}

void Peer::SetReceiveCallback(void (*callback)(Peer *, ByteReader &, Flags))
{
	callbackOnReceive = callback;
}

void Peer::CallCallbackReceive(ISteamNetworkingMessage *packet)
{
	ByteReader reader(packet, 0);
	if (callbackOnReceive) {
		Flags flags = 0;
		if (packet->m_nFlags & k_nSteamNetworkingSend_Reliable) {
			flags = FLAG_RELIABLE;
		}

		callbackOnReceive(this, reader, flags);
	}
}

void Peer::SetDisconnect(void (*callback)(Peer *))
{
	callbackOnDisconnect = callback;
}

void Peer::CallCallbackDisconnect()
{
	if (callbackOnDisconnect) {
		callbackOnDisconnect(this);
	} else if (host->callbackOnDisconnect) {
		host->callbackOnDisconnect(this);
	}
}

void Peer::SetReadyToUse() { readyToUse = true; }

uint32_t Peer::GetRoundTripTime() const
{
	SteamNetConnectionRealTimeStatus_t status;
	host->host->GetConnectionRealTimeStatus(peer, &status, 0, nullptr);
	return status.m_nPing;
}
} // namespace icon6
