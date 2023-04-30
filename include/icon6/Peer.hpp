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

namespace icon6 {
	
	enum MessageFlags : uint32_t {
		FLAG_SEQUENCED = 1<<0,
		FLAG_RELIABLE = 1<<1,
	};
	
	class Host;
	
	class Peer final : public std::enable_shared_from_this<Peer> {
	public:
		
		~Peer();
		
		void Send(const void* data, uint32_t bytes, uint32_t flags);
		void Disconnect(uint32_t disconnectData);
		
		inline uint32_t GetMTU() const { return peer->mtu; }
		inline uint32_t GetEncryptedMTU() const { return peer->mtu-12-16-4; }
		inline uint32_t GetRoundtripTime() const {
			return peer->roundTripTime >= 2 ? peer->roundTripTime : 2;
		}
		inline uint32_t GetLatency() const { return GetRoundtripTime()>>1; }
		
		inline bool IsValid() const { return peer && IsHandshakeDone(); }
		inline operator bool() const { return peer && IsHandshakeDone(); }
		
		inline bool IsHandshakeDone() const {
			return state == STATE_READY_TO_USE;
		}
		
		friend class Host;
		
		inline std::shared_ptr<Host> GetHost() { return host; }
		
		void SetReceiveCallback(void(*callback)(Peer*, void* data,
					uint32_t size, uint32_t flags));
		void SetDisconnect(void(*callback)(Peer*, uint32_t disconnectData));
		
	public:
		
		void* userData;
		std::shared_ptr<void> userSharedPointer;
		
	private:
		
		void StartHandshake();
		
		void CallCallbackReceive(void* data, uint32_t size, uint32_t flags);
		void CallCallbackDisconnect(uint32_t data);
		
		void Destroy();
		
		enum ENetPacketFlags {
			INTERNAL_SEQUENCED = ENET_PACKET_FLAG_RELIABLE,
			INTERNAL_UNSEQUENCED = ENET_PACKET_FLAG_RELIABLE | ENET_PACKET_FLAG_UNSEQUENCED,
			INTERNAL_UNRELIABLE = 0,
		};
		
		enum PeerConnectionState : uint32_t {
			STATE_NONE = 0,
			
			STATE_ZOMBIE = 1,
			STATE_DISCONNECTED = 2,
			
			STATE_RECEIVED_CERT = 3,
			STATE_SENT_CERT = 4,
			STATE_SENT_AND_RECEIVED_CERT = 5,
			STATE_AUTHENTICATED = 6,
			
			STATE_BEFORE_ON_CONNECT_CALLBACK = 7,
			STATE_READY_TO_USE = 8,
		};
		
		Peer(std::shared_ptr<Host> host, ENetPeer* peer);
		
	private:
		
		std::shared_ptr<Host> host;
		ENetPeer* peer;
		uint32_t state;
		
		void(*callbackOnReceive)(Peer*, void* data, uint32_t size,
					uint32_t flags);
		void(*callbackOnDisconnect)(Peer*, uint32_t disconnectData);
		
		
	};
}

#endif

