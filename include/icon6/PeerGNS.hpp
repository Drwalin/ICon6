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

#ifndef ICON6_PEER_GAME_NETWORKING_SOCKETS_HPP
#define ICON6_PEER_GAME_NETWORKING_SOCKETS_HPP

#include <cinttypes>

#include <vector>
#include <queue>

#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

#include "Flags.hpp"
#include "ByteReader.hpp"

#include "Peer.hpp"

namespace icon6
{
class Host;

namespace gns
{

class Peer final : public icon6::Peer
{
public:
	virtual ~Peer() override;

	virtual uint32_t GetRoundTripTime() const override;

	SteamNetConnectionRealTimeStatus_t GetRealTimeStats();

public:
	// thread unsafe
	virtual bool _InternalSend(const void *data, uint32_t length, Flags flags) override;
	// thread unsafe
	virtual void _InternalDisconnect() override;

	friend class icon6::Host;

private:
	void CallCallbackReceive(ISteamNetworkingMessage *msg);
	void CallCallbackDisconnect();

protected:
	Peer(Host *host, HSteamNetConnection connection);

private:

	HSteamNetConnection peer;
};
}
} // namespace icon6

#endif
