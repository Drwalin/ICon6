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

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

#include "Flags.hpp"

namespace icon6
{
class Host;

class Peer final
{
public:
	~Peer();

	void Send(std::vector<uint8_t> &&data, Flags flags);

	void Disconnect(uint32_t disconnectData);

	inline uint32_t GetMTU() const {
		// TODO: get mtu of connection
		return 1400;
	}
	static constexpr uint32_t GetEncryptedMessageOverhead()
	{
		// TODO: find packet overhead
		return 64;
	}
	inline uint32_t GetEncryptedMTU() const
	{
		// TODO: get mtu of connection
		return GetMTU() - GetEncryptedMessageOverhead();
	}
	inline uint32_t GetMaxSinglePackedMessageSize() const
	{
		// TODO: find this value
		return GetEncryptedMTU() - 64;
	}
	inline uint32_t GetRoundtripTime() const
	{
		// TODO: implement
		throw "Peer::GetRoundTripUnimplemented";
	}
	inline uint32_t GetLatency() const { return GetRoundtripTime() >> 1; }

	inline bool IsValid() const {
		// TODO: implement
		return true;
	}

	inline Host *GetHost() { return host; }

	void SetReceiveCallback(void (*callback)(Peer *, std::vector<uint8_t> &data,
											 Flags flags));
	void SetDisconnect(void (*callback)(Peer *));

	friend class Host;
	friend class ConnectionEncryptionState;

public:
	void *userData;
	std::shared_ptr<void> userSharedPointer;

public:
	void _InternalSend(std::vector<uint8_t> &&data, Flags flags);
	void _InternalDisconnect();
	void _InternalStartHandshake();

private:
	void CallCallbackReceive(ISteamNetworkingMessage *packet, Flags flags);
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

	void (*callbackOnReceive)(Peer *, std::vector<uint8_t> &data, Flags flags);
	void (*callbackOnDisconnect)(Peer *);
};
} // namespace icon6

#endif
