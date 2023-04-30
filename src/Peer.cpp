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

#include <string>
#include <thread>
#include <memory>

#include "../include/icon6/Host.hpp"
#include "../include/icon6/Command.hpp"

#include "../include/icon6/Peer.hpp"

namespace icon6 {
	Peer::Peer(std::shared_ptr<Host> host, ENetPeer* peer) : host(host),
			peer(peer) {
		DEBUG("SETTING peer->data TO this");
		peer->data = this;
		state = STATE_NONE;
		callbackOnReceive = host->callbackOnReceive;
		callbackOnDisconnect = host->callbackOnDisconnect;
		DEBUG("");
	}
	Peer::~Peer() {
	}
	
	void Peer::Destroy() {
		DEBUG("");
		peer->data = nullptr;
		peer = nullptr;
		host = nullptr;
	}
	
	void Peer::Send(const void* data, uint32_t bytes, uint32_t flags) {
		DEBUG("INIT SEND FROM %i TO %i", host->host->address.port, peer->address.port);
		Command com;
		com.binaryData.insert(com.binaryData.begin(), (const uint8_t*)data, (const uint8_t*)data+bytes);
		com.flags = flags;
		com.peer = shared_from_this();
		com.host = host;
		com.callback = [](Command& com) {
		DEBUG("CALLBACK SENDING %i TO %i", com.host->host->address.port, com.peer->peer->address.port);
			// TODO: do encryption in place for com.binaryData
// 			struct PacketWithData {
// 				ENetPacket packet;
// 				std::vector<uint8_t> data;
// 				std::shared_ptr<Host> host;
// 			};
// 			PacketWithData *packet = new PacketWithData{{},
// 				std::move(com.binaryData), com.host};
// 			packet->packet.data = packet->data.data();
// 			packet->packet.dataLength = packet->data.size();
// 			packet->packet.referenceCount = 0;
// 			packet->packet.flags = 0 |
// 				(com.flags&FLAG_SEQUENCED ? 0 : ENET_PACKET_FLAG_UNSEQUENCED) |
// 				(com.flags&FLAG_RELIABLE ? ENET_PACKET_FLAG_RELIABLE : 0);
// 			packet->packet.userData = packet;
// 			packet->packet.freeCallback = [](ENetPacket*p) {
// 		DEBUG("CALLBACK DELETE PACKET");
// 				delete (PacketWithData*)(p->userData);
// 			};
// 			DEBUG("                     packet pointer = %p", &(packet->packet));
// 			enet_peer_send(com.peer->peer,
// 					com.flags&(FLAG_RELIABLE|FLAG_SEQUENCED) ? 0 : 1,
// 					&(packet->packet));
			
			ENetPacket* packet = enet_packet_create(com.binaryData.data(),
				com.binaryData.size(), 0 |
				(com.flags&FLAG_SEQUENCED ? 0 : ENET_PACKET_FLAG_UNSEQUENCED) |
				(com.flags&FLAG_RELIABLE ? ENET_PACKET_FLAG_RELIABLE : 0));
			
			enet_peer_send(com.peer->peer,
					com.flags&(FLAG_RELIABLE|FLAG_SEQUENCED) ? 0 : 1, packet);
		};
		host->EnqueueCommand(std::move(com));
	}
	
	void Peer::Disconnect(uint32_t disconnectData) {
		DEBUG("INIT DISCONNECT PEER %i FROM HOST %i", peer->address.port, host->host->address.port);
		Command com;
		// TODO: do encryption instead of just copying
		com.peer = shared_from_this();
		com.host = host;
		com.callback = [](Command& com) {
		DEBUG("EXECUTING ENET DISCONNECT PEER %i FROM HOST %i", com.peer->peer->address.port, com.host->host->address.port);
			enet_peer_disconnect(com.peer->peer, 0);
		};
	}
	
	void Peer::SetReceiveCallback(void(*callback)(Peer*, void* data,
				uint32_t size, uint32_t flags)) {
		callbackOnReceive = callback;
	}
	
	void Peer::StartHandshake() {
		DEBUG("START HANDSHAKE");
		// TODO: implement handshake for encryption
		state = STATE_BEFORE_ON_CONNECT_CALLBACK;
		if(host->callbackOnConnect)
			host->callbackOnConnect(this);
		state = STATE_READY_TO_USE;
		DEBUG("AFTER HANDSHAKE AND CONNECTION CALLBACK        --- STATE = %i", peer->address.port, host->host->address.port, state);
	}
	
	void Peer::CallCallbackReceive(void* data, uint32_t size, uint32_t flags) {
		DEBUG("CALLBACK RECEIVE FROM PEER %i FROM HOST %i    --- STATE = %i", peer->address.port, host->host->address.port, state);
		switch(state) {
		case STATE_READY_TO_USE:
		DEBUG("CALLBACK RECEIVE STATE_READY_TO_USE FROM PEER %i FROM HOST %i", peer->address.port, host->host->address.port);
			if(callbackOnReceive) {
				callbackOnReceive(this, data, size, flags);
			} else if(host->callbackOnReceive) {
				host->callbackOnReceive(this, data, size, flags);
			}
			break;
		default:
		DEBUG("CALLBACK RECEIVE INVALID STATE FROM PEER %i FROM HOST %i", peer->address.port, host->host->address.port);
			throw "icon6::Peer::CallCallbackReceive other states handling is not implemented yet";
		}
	}
	
	void Peer::SetDisconnect(void(*callback)(Peer*, uint32_t disconnectData)) {
		callbackOnDisconnect = callback;
	}
	
	void Peer::CallCallbackDisconnect(uint32_t data) {
		DEBUG("CALLBACK DISCONNECT PEER %i FROM HOST %i", peer->address.port, host->host->address.port);
		if(callbackOnDisconnect) {
			callbackOnDisconnect(this, data);
		} else if(host->callbackOnDisconnect) {
			host->callbackOnDisconnect(this, data);
		}
	}
}

