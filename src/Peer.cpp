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

#include <thread>
#include <memory>
#include <cstring>

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

void Peer::_InternalSend(std::vector<uint8_t> &data, Flags flags)
{
	int steamFlags = 0;
	if (flags & FLAG_RELIABLE) {
		steamFlags = k_nSteamNetworkingSend_Reliable;
	} else {
		steamFlags = k_nSteamNetworkingSend_Unreliable;
	}
	auto result = host->host->SendMessageToConnection(peer, data.data(), data.size(), steamFlags,
										nullptr);
	if (result == k_EResultLimitExceeded) {
		Send(std::move(data), flags);
		return;
	}
}

void Peer::_InternalSend(const void *data, uint32_t length, const Flags flags)
{
	int steamFlags = 0;
	if (flags & FLAG_RELIABLE) {
		steamFlags = k_nSteamNetworkingSend_Reliable;
	} else {
		steamFlags = k_nSteamNetworkingSend_Unreliable;
	}
	auto result = host->host->SendMessageToConnection(peer, data, length, steamFlags,
										nullptr);
	if (result == k_EResultLimitExceeded) {
		Send(std::vector<uint8_t>((uint8_t*)data, (uint8_t*)data+length), flags);
		return;
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
