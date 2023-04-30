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
		peer->data = this;
		state = STATE_NONE;
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
	
	void Peer::Send(const void* data, uint32_t bytes, uint32_t flags) {
		Command com;
		com.binaryData.insert(com.binaryData.begin(), (const uint8_t*)data, (const uint8_t*)data+bytes);
		com.flags = flags;
		com.peer = shared_from_this();
		com.host = host;
		com.callback = [](Command& com) {
			// TODO: do encryption in place for com.binaryData
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
		Command com;
		// TODO: do encryption instead of just copying
		com.peer = shared_from_this();
		com.host = host;
		com.callback = [](Command& com) {
			enet_peer_disconnect(com.peer->peer, 0);
		};
	}
	
	void Peer::SetReceiveCallback(void(*callback)(Peer*, void* data,
				uint32_t size, uint32_t flags)) {
		callbackOnReceive = callback;
	}
	
	void Peer::StartHandshake() {
		// TODO: implement handshake for encryption
		state = STATE_BEFORE_ON_CONNECT_CALLBACK;
		if(host->callbackOnConnect)
			host->callbackOnConnect(this);
		state = STATE_READY_TO_USE;
	}
	
	void Peer::CallCallbackReceive(void* data, uint32_t size, uint32_t flags) {
		switch(state) {
		case STATE_READY_TO_USE:
			if(callbackOnReceive) {
				callbackOnReceive(this, data, size, flags);
			} else if(host->callbackOnReceive) {
				host->callbackOnReceive(this, data, size, flags);
			}
			break;
		default:
			throw "icon6::Peer::CallCallbackReceive other states handling is not implemented yet";
		}
	}
	
	void Peer::SetDisconnect(void(*callback)(Peer*, uint32_t disconnectData)) {
		callbackOnDisconnect = callback;
	}
	
	void Peer::CallCallbackDisconnect(uint32_t data) {
		if(callbackOnDisconnect) {
			callbackOnDisconnect(this, data);
		} else if(host->callbackOnDisconnect) {
			host->callbackOnDisconnect(this, data);
		}
	}
}

