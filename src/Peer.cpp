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
#include "../include/icon6/Cert.hpp"
#include "../include/icon6/Host.hpp"
#include "../include/icon6/Command.hpp"
#include "../include/icon6/Cert.hpp"

#include "../include/icon6/Peer.hpp"

namespace icon6 {
	Peer::Peer(std::shared_ptr<Host> host, ENetPeer* peer) : host(host),
			peer(peer) {
		peer->data = this;
		callbackOnReceive = host->callbackOnReceive;
		callbackOnDisconnect = host->callbackOnDisconnect;
	}
	Peer::~Peer() {
	}
	
	void Peer::Destroy() {
		peer->data = nullptr;
		peer = nullptr;
		host = nullptr;
	}
	
	void Peer::Send(std::vector<uint8_t>&& data, uint32_t flags) {
		if(state != STATE_READY_TO_USE) {
			throw "Peer::Send Handshake is not finished yet. Sending is not posible.";
		}
		Command command{commands::ExecuteSend{}};
		commands::ExecuteSend& com = command.executeSend;
		com.flags = flags;
		com.peer = shared_from_this();
		com.data = std::move(data);
		host->EnqueueCommand(std::move(command));
	}
	
	void Peer::_InternalSend(std::vector<uint8_t>& data, uint32_t flags)
	{
		const uint32_t packetSize = ConnectionEncryptionState
			::GetEncryptedMessageLength(data.size());
		
		ENetPacket* packet = enet_packet_create(nullptr,
			packetSize, 0 |
			(flags&FLAG_SEQUENCED ? 0 : ENET_PACKET_FLAG_UNSEQUENCED) |
			(flags&FLAG_RELIABLE ? ENET_PACKET_FLAG_RELIABLE : 0));
		
		EncryptMessage(packet->data, data.data(), data.size(), flags);
		
		uint8_t channel = flags&(FLAG_RELIABLE|FLAG_SEQUENCED) ? 0 : 1;
		enet_peer_send(peer, channel, packet);
	}
	
	void Peer::Disconnect(uint32_t disconnectData)
	{
		Command command{commands::ExecuteDisconnect{}};
		commands::ExecuteDisconnect& com = command.executeDisconnect;
		com.peer = shared_from_this();
		com.disconnectData = disconnectData;
		host->EnqueueCommand(std::move(command));
	}
	
	void Peer::_InternalDisconnect(uint32_t disconnectData)
	{
		enet_peer_disconnect(peer, disconnectData);
	}
	
	void Peer::SetReceiveCallback(void(*callback)(Peer*,
				std::vector<uint8_t>& data, uint32_t flags)) {
		callbackOnReceive = callback;
	}
	
	void Peer::CallCallbackReceive(uint8_t* data, uint32_t size, uint32_t flags) {
		switch(state) {
		case STATE_SENT_CERT:
			ReceivedWhenStateSentCert(data, size, flags);
			break;
			
		case STATE_SENT_KEX:
			ReceivedWhenStateSentKex(data, size, flags);
			
			if(state == STATE_BEFORE_ON_CONNECT_CALLBACK) {
				if(host->callbackOnConnect)
					host->callbackOnConnect(this);
				if(state == STATE_BEFORE_ON_CONNECT_CALLBACK)
					state = STATE_READY_TO_USE;
			}
			break;
			
		case STATE_READY_TO_USE:
			{
				DecryptMessage(receivedData, data, size, flags);
				if(state == STATE_READY_TO_USE) {
					if(callbackOnReceive) {
						callbackOnReceive(this, receivedData, flags);
					}
				}
				fflush(stdout);
			} break;
			
		case STATE_ZOMBIE:
		case STATE_DISCONNECTED:
		case STATE_FAILED_TO_AUTHENTICATE:
		case STATE_FAILED_TO_VERIFY_MESSAGE:
			break;
		default:
			DEBUG("icon6::Peer::CallCallbackReceive other states handling is invalid");
			throw "icon6::Peer::CallCallbackReceive other states handling is invalid";
		}
	}
	
	void Peer::SetDisconnect(void(*callback)(Peer*, uint32_t disconnectData)) {
		callbackOnDisconnect = callback;
	}
	
	void Peer::CallCallbackDisconnect(uint32_t data) {
		if(state == STATE_READY_TO_USE) {
			if(callbackOnDisconnect) {
				callbackOnDisconnect(this, data);
			} else if(host->callbackOnDisconnect) {
				host->callbackOnDisconnect(this, data);
			}
		}
	}
}

