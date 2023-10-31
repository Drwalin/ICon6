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

#ifndef ICON6_PEER_HPP
#define ICON6_PEER_HPP

#include <cinttypes>

#include <memory>
#include <vector>

#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

#include "Flags.hpp"
#include "ByteReader.hpp"

namespace icon6
{
class Host;

class Peer final
{
public:
	~Peer();

	void Send(std::vector<uint8_t> &&data, Flags flags);

	void Disconnect();

	static constexpr uint32_t GetMaxMessagePayload() { return 1200; }
	uint32_t GetRoundTripTime() const;

	inline bool IsReadyToUse() const { return readyToUse; }

	inline Host *GetHost() { return host; }

	void SetReceiveCallback(void (*callback)(Peer *, ByteReader &,
											 Flags flags));
	void SetDisconnect(void (*callback)(Peer *));

public:
	uint64_t userData;
	void *userPointer;
	std::shared_ptr<void> userSharedPointer;

	friend class Host;
	friend class ConnectionEncryptionState;

public:
	// thread unsafe
	void _InternalSend(const void *data, uint32_t length, Flags flags);
	// thread unsafe
	void _InternalDisconnect();

private:
	void SetReadyToUse();

	void CallCallbackReceive(ISteamNetworkingMessage *msg);
	void CallCallbackDisconnect();

	enum SteamMessageFlags {
		INTERNAL_SEQUENCED = k_nSteamNetworkingSend_Reliable,
		INTERNAL_UNRELIABLE = k_nSteamNetworkingSend_UnreliableNoNagle,
	};

protected:
	Peer(Host *host, HSteamNetConnection connection);

private:
	std::vector<uint8_t> receivedData;

	Host *host;
	HSteamNetConnection peer;

	void (*callbackOnReceive)(Peer *, ByteReader &, Flags);
	void (*callbackOnDisconnect)(Peer *);

	bool readyToUse;
};
} // namespace icon6

#endif
