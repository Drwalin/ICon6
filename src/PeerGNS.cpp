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
#include "../include/icon6/HostGNS.hpp"
#include "../include/icon6/Command.hpp"

#include "../include/icon6/PeerGNS.hpp"

namespace icon6
{
namespace gns
{
SteamNetConnectionRealTimeStatus_t Peer::GetRealTimeStats()
{
	SteamNetConnectionRealTimeStatus_t status;
	((gns::Host *)host)
		->host->GetConnectionRealTimeStatus(peer, &status, 0, nullptr);
	return status;
}

Peer::Peer(Host *host, HSteamNetConnection connection)
	: icon6::Peer(host), peer(connection)
{
	userData = 0;
	userPointer = nullptr;
	host->host->SetConnectionUserData(peer, (uint64_t)(size_t)this);
	callbackOnReceive = host->callbackOnReceive;
	callbackOnDisconnect = host->callbackOnDisconnect;
	readyToUse = false;
}
Peer::~Peer() {}

bool Peer::_InternalSend(const void *data, uint32_t length, const Flags flags)
{
	auto result = ((gns::Host *)host)
					  ->host->SendMessageToConnection(peer, data, length,
													  flags.field, nullptr);
	if (result == k_EResultLimitExceeded) {
		return false;
	}
	return true;
}

void Peer::_InternalDisconnect()
{
	if (peer) {
		((gns::Host *)host)->host->CloseConnection(peer, 0, nullptr, true);
	}
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

void Peer::CallCallbackDisconnect()
{
	if (callbackOnDisconnect) {
		callbackOnDisconnect(this);
	} else if (((gns::Host *)host)->callbackOnDisconnect) {
		((gns::Host *)host)->callbackOnDisconnect(this);
	}
}

uint32_t Peer::GetRoundTripTime() const
{
	SteamNetConnectionRealTimeStatus_t status;
	((gns::Host *)host)
		->host->GetConnectionRealTimeStatus(peer, &status, 0, nullptr);
	return status.m_nPing;
}
} // namespace gns
} // namespace icon6
