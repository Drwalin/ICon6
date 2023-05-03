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

#include "../include/icon6/Host.hpp"
#include "../include/icon6/Command.hpp"

#include "../include/icon6/Peer.hpp"
#include "icon6/Cert.hpp"

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
		if(state != STATE_READY_TO_USE) {
			throw "Peer::Send Handshake is not finished yet. Sending is not posible.";
		}
		Command com;
		com.binaryData.insert(com.binaryData.begin(), (uint8_t*)data,
				(uint8_t*)data+bytes);
		com.flags = flags;
		com.peer = shared_from_this();
		com.host = host;
		com.callback = [](Command& com) {
			// TODO: do encryption in place for com.binaryData
			constexpr uint32_t nonceSize = sizeof(sendMessagesCounter);
			constexpr uint32_t macSize
				= crypto_aead_chacha20poly1305_ietf_ABYTES;
			const uint32_t packetSize = com.binaryData.size() + nonceSize
				+ macSize;
			uint8_t channel = com.flags&(FLAG_RELIABLE|FLAG_SEQUENCED) ? 0 : 1;
			ENetPacket* packet = enet_packet_create(com.binaryData.data(),
				packetSize, 0 |
				(com.flags&FLAG_SEQUENCED ? 0 : ENET_PACKET_FLAG_UNSEQUENCED) |
				(com.flags&FLAG_RELIABLE ? ENET_PACKET_FLAG_RELIABLE : 0));
			uint8_t ad[sizeof(sendMessagesCounter)];
			*(decltype(sendMessagesCounter)*)ad = htonl(com.flags);
			com.peer->sendMessagesCounter++;
			
			
			uint8_t nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES];
			memset(nonce, 0, sizeof(nonce));
			*(decltype(sendMessagesCounter)*)nonce
				= htonl(com.peer->sendMessagesCounter);
			*(decltype(sendMessagesCounter)*)(packet->data)
				= htonl(com.peer->sendMessagesCounter);
			
			crypto_aead_chacha20poly1305_ietf_encrypt_detached(
					packet->data+nonceSize,
					packet->data+nonceSize+com.binaryData.size(), nullptr,
					com.binaryData.data(), com.binaryData.size(),
					ad, sizeof(ad),
					nullptr,
					nonce, com.peer->sendingKey);
			
			enet_peer_send(com.peer->peer,
					channel, packet);
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
	
	void Peer::SetReceiveCallback(void(*callback)(Peer*,
				std::vector<uint8_t>& data, uint32_t flags)) {
		callbackOnReceive = callback;
	}
	
	void Peer::StartHandshake() {
		// TODO: send certificate instead of only public key
		
		uint8_t data[crypto::SIGN_PUBLIC_KEY_BYTES];
		host->certKey->CopyPublicKey(data);
		ENetPacket* packet = enet_packet_create(data, sizeof(data),
				ENET_PACKET_FLAG_RELIABLE);
		enet_peer_send(peer, 0, packet);
		state = STATE_SENT_CERT;
	}
	
	void Peer::CallCallbackReceive(void* data, uint32_t size, uint32_t flags) {
		switch(state) {
		case STATE_SENT_CERT:
			if(size != crypto::SIGN_PUBLIC_KEY_BYTES) {
				DEBUG("Received size of certificate (public key) is invalid");
				state = STATE_FAILED_TO_AUTHENTICATE;
				enet_peer_disconnect(peer, 1);
				return;
			}
			memcpy(peerPublicKey, data, size);
			state = STATE_SENT_AND_RECEIVED_CERT;
			
			{
				uint8_t data[crypto::KEX_PUBLIC_KEY_BYTES
					+ crypto::SIGNATURE_BYTES];
				crypto_kx_keypair(publicKey, secretKey);
				memcpy(data, publicKey, crypto::KEX_PUBLIC_KEY_BYTES);
				host->certKey->Sign(data+crypto::KEX_PUBLIC_KEY_BYTES,
						data, crypto::KEX_PUBLIC_KEY_BYTES);
				ENetPacket* packet = enet_packet_create(data, sizeof(data),
						ENET_PACKET_FLAG_RELIABLE);
				enet_peer_send(peer, 0, packet);
				state = STATE_SENT_KEX;
			}
			
			break;
		case STATE_SENT_KEX:
			if(size != crypto::KEX_PUBLIC_KEY_BYTES + crypto::SIGNATURE_BYTES) {
				DEBUG("Received kex size is invalid");
				state = STATE_FAILED_TO_AUTHENTICATE;
				enet_peer_disconnect(peer, 1);
				return;
			}
			if(crypto_sign_ed25519_verify_detached((uint8_t*)data+crypto::KEX_PUBLIC_KEY_BYTES,
						(uint8_t*)data, crypto::KEX_PUBLIC_KEY_BYTES,
						peerPublicKey)) {
				DEBUG("Failed to verify kex message");
				state = STATE_FAILED_TO_AUTHENTICATE;
				enet_peer_disconnect(peer, 2);
				return;
			}
			state = STATE_SENT_AND_RECEIVED_KEX;
			
			{
				uint8_t *rx = receivingKey;
				uint8_t *tx = sendingKey;
				uint8_t *clientPublicKey = (uint8_t*)data;
				if(memcmp(publicKey, clientPublicKey,
							crypto::KEX_PUBLIC_KEY_BYTES) < 0) {
					if(crypto_kx_client_session_keys(rx, tx, publicKey,
								secretKey, clientPublicKey)) {
						DEBUG("Failed to generate client session key");
						state = STATE_FAILED_TO_AUTHENTICATE;
						enet_peer_disconnect(peer, 2);
						return;
					}
				} else {
					if(crypto_kx_server_session_keys(rx, tx, publicKey,
								secretKey, clientPublicKey)) {
						DEBUG("Failed to generate server session key");
						state = STATE_FAILED_TO_AUTHENTICATE;
						enet_peer_disconnect(peer, 2);
						return;
					}
				}
			}
		
			state = STATE_BEFORE_ON_CONNECT_CALLBACK;
			if(host->callbackOnConnect)
				host->callbackOnConnect(this);
			if(state == STATE_BEFORE_ON_CONNECT_CALLBACK)
				state = STATE_READY_TO_USE;
			break;
		case STATE_READY_TO_USE:
			{
				constexpr uint32_t nonceSize = sizeof(sendMessagesCounter);
				constexpr uint32_t macSize
					= crypto_aead_chacha20poly1305_ietf_ABYTES;
				uint8_t nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES];
				memset(nonce, 0, sizeof(nonce));
				memcpy(nonce, data, nonceSize);
				
				uint8_t ad[sizeof(sendMessagesCounter)];
				*(decltype(sendMessagesCounter)*)ad = htonl(flags);
				
				
				receivedData.resize(size-nonceSize
							-crypto_aead_chacha20poly1305_ietf_ABYTES);
				
				if(crypto_aead_chacha20poly1305_ietf_decrypt_detached(
						receivedData.data(), nullptr,
						(uint8_t*)data+nonceSize,
						receivedData.size(),
						(uint8_t*)data+size-macSize,
						ad, sizeof(ad),
						nonce,
						receivingKey)) {
					DEBUG("Failed to decrypt `%s`", receivedData.data());
					state = STATE_FAILED_TO_VERIFY_MESSAGE;
					enet_peer_disconnect(peer, 2);
					return;
				}
				
				if(callbackOnReceive) {
					callbackOnReceive(this, receivedData, flags);
				} else if(host->callbackOnReceive) {
					host->callbackOnReceive(this, receivedData, flags);
				}
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

