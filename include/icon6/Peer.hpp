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

#include <enet/enet.h>

#include <cinttypes>

#include <thread>
#include <memory>
#include <atomic>

#include "ConnectionEncryptionState.hpp"

namespace icon6
{

enum MessageFlags : uint32_t {
	FLAG_SEQUENCED = 1 << 0,
	FLAG_RELIABLE = 1 << 1,
};

class Host;

class Peer final : public ConnectionEncryptionState
{
public:
	~Peer();

	void Send(std::vector<uint8_t> &&data, uint32_t flags);
	inline void SendReliableSequenced(std::vector<uint8_t> &&data);
	inline void SendReliableUnsequenced(std::vector<uint8_t> &&data);
	inline void SendUnreliableSequenced(std::vector<uint8_t> &&data);
	inline void SendUnreliableUnsequenced(std::vector<uint8_t> &&data);
	inline void Send(const void *data, uint32_t bytes, uint32_t flags);
	inline void SendReliableSequenced(const void *data, uint32_t bytes);
	inline void SendReliableUnsequenced(const void *data, uint32_t bytes);
	inline void SendUnreliableSequenced(const void *data, uint32_t bytes);
	inline void SendUnreliableUnsequenced(const void *data, uint32_t bytes);

	void Disconnect(uint32_t disconnectData);

	inline uint32_t GetMTU() const { return peer->mtu; }
	inline uint32_t GetEncryptedMTU() const
	{
		return peer->mtu -
			   ConnectionEncryptionState::GetEncryptedMessageOverhead();
	}
	inline uint32_t GetRoundtripTime() const
	{
		return peer->roundTripTime >= 2 ? peer->roundTripTime : 2;
	}
	inline uint32_t GetLatency() const { return GetRoundtripTime() >> 1; }

	inline bool IsValid() const { return peer && IsHandshakeDone(); }
	inline operator bool() const { return peer && IsHandshakeDone(); }

	inline bool IsHandshakeDone() const { return state == STATE_READY_TO_USE; }

	friend class Host;
	friend class ConnectionEncryptionState;

	inline std::shared_ptr<Host> GetHost() { return host; }

	void SetReceiveCallback(void (*callback)(Peer *, std::vector<uint8_t> &data,
											 uint32_t flags));
	void SetDisconnect(void (*callback)(Peer *, uint32_t disconnectData));

public:
	void *userData;
	std::shared_ptr<void> userSharedPointer;

public:
	void _InternalSend(std::vector<uint8_t> &data, uint32_t flags);
	void _InternalDisconnect(uint32_t disconnectData);

private:
	void CallCallbackReceive(uint8_t *data, uint32_t size, uint32_t flags);
	void CallCallbackDisconnect(uint32_t data);

	void Destroy();

	enum ENetPacketFlags {
		INTERNAL_SEQUENCED = ENET_PACKET_FLAG_RELIABLE,
		INTERNAL_UNSEQUENCED =
			ENET_PACKET_FLAG_RELIABLE | ENET_PACKET_FLAG_UNSEQUENCED,
		INTERNAL_UNRELIABLE = 0,
	};

protected:
	Peer(std::shared_ptr<Host> host, ENetPeer *peer);

private:
	std::vector<uint8_t> receivedData;

	std::shared_ptr<Host> host;
	ENetPeer *peer;

	void (*callbackOnReceive)(Peer *, std::vector<uint8_t> &data,
							  uint32_t flags);
	void (*callbackOnDisconnect)(Peer *, uint32_t disconnectData);
};

inline void Peer::SendReliableSequenced(std::vector<uint8_t> &&data)
{
	Send(std::move(data), FLAG_RELIABLE | FLAG_SEQUENCED);
}

inline void Peer::SendReliableUnsequenced(std::vector<uint8_t> &&data)
{
	Send(std::move(data), FLAG_RELIABLE);
}

inline void Peer::SendUnreliableSequenced(std::vector<uint8_t> &&data)
{
	Send(std::move(data), FLAG_SEQUENCED);
}

inline void Peer::SendUnreliableUnsequenced(std::vector<uint8_t> &&data)
{
	Send(std::move(data), 0);
}

inline void Peer::Send(const void *data, uint32_t bytes, uint32_t flags)
{
	Send(std::vector<uint8_t>((uint8_t *)data, (uint8_t *)data + bytes), flags);
}

inline void Peer::SendReliableSequenced(const void *data, uint32_t bytes)
{
	Send(data, bytes, FLAG_RELIABLE | FLAG_SEQUENCED);
}

inline void Peer::SendReliableUnsequenced(const void *data, uint32_t bytes)
{
	Send(data, bytes, FLAG_RELIABLE);
}

inline void Peer::SendUnreliableSequenced(const void *data, uint32_t bytes)
{
	Send(data, bytes, FLAG_SEQUENCED);
}

inline void Peer::SendUnreliableUnsequenced(const void *data, uint32_t bytes)
{
	Send(data, bytes, 0);
}
} // namespace icon6

#endif
