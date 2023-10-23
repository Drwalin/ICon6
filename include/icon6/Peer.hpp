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

#include <enet/enet.h>

#include "Flags.hpp"
#include "ConnectionEncryptionState.hpp"

namespace icon6
{

class Host;

class Peer final
{
public:
	~Peer();

	void Send(std::vector<uint8_t> &&data, Flags flags);

	void Disconnect(uint32_t disconnectData);

	inline uint32_t GetMTU() const { return peer->mtu; }
	inline uint32_t GetEncryptedMTU() const
	{
		return peer->mtu - PeerEncryptor::GetEncryptedMessageOverhead();
	}
	inline uint32_t GetMaxSinglePackedMessageSize() const
	{
		return GetEncryptedMTU() -
			   sizeof(ENetProtocolHeader)		  // ENet protocol overhead
			   - sizeof(ENetProtocolSendFragment) // ENet protocol overhead
			   - sizeof(uint32_t) // ENet protocol checksum overhead
			;
	}
	inline uint32_t GetRoundtripTime() const
	{
		return peer->roundTripTime >= 2 ? peer->roundTripTime : 2;
	}
	inline uint32_t GetLatency() const { return GetRoundtripTime() >> 1; }

	inline bool IsValid() const { return peer != nullptr && IsHandshakeDone(); }

	inline PeerConnectionState GetState() const
	{
		return encryptionState.state;
	}
	inline bool IsHandshakeDone() const
	{
		return GetState() == STATE_READY_TO_USE;
	}

	inline Host *GetHost() { return host; }

	void SetReceiveCallback(void (*callback)(Peer *, std::vector<uint8_t> &data,
											 Flags flags));
	void SetDisconnect(void (*callback)(Peer *, uint32_t disconnectData));

	friend class Host;
	friend class ConnectionEncryptionState;

public:
	void *userData;
	std::shared_ptr<void> userSharedPointer;

public:
	void _InternalSend(std::vector<uint8_t> &&data, Flags flags);
	void _InternalDisconnect(uint32_t disconnectData);
	void _InternalStartHandshake();

private:
	void CallCallbackReceive(ENetPacket *packet, Flags flags);
	void CallCallbackDisconnect(uint32_t data);

	enum ENetPacketFlags {
		INTERNAL_SEQUENCED = ENET_PACKET_FLAG_RELIABLE,
		INTERNAL_UNSEQUENCED =
			ENET_PACKET_FLAG_RELIABLE | ENET_PACKET_FLAG_UNSEQUENCED,
		INTERNAL_UNRELIABLE = 0,
	};

protected:
	Peer(Host *host, ENetPeer *peer);

private:
	ConnectionEncryptionState encryptionState;

	std::vector<uint8_t> receivedData;

	Host *host;
	ENetPeer *peer;

	void (*callbackOnReceive)(Peer *, std::vector<uint8_t> &data, Flags flags);
	void (*callbackOnDisconnect)(Peer *, uint32_t disconnectData);
};

} // namespace icon6

#endif
