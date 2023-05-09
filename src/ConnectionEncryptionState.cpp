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
#include <cstddef>

#include "../include/icon6/Host.hpp"
#include "../include/icon6/Command.hpp"
#include "../include/icon6/Cert.hpp"

#include "../include/icon6/ConnectionEncryptionState.hpp"

namespace icon6 {
	ConnectionEncryptionState::ConnectionEncryptionState() {
		sodium_mlock(this, sizeof(*this));
		state = STATE_NONE;
	}
	ConnectionEncryptionState::~ConnectionEncryptionState() {
		sodium_memzero(this, sizeof(*this));
		sodium_munlock(this, sizeof(*this));
	}
	
	
	void ConnectionEncryptionState::EncryptMessage(uint8_t* cipher,
			const uint8_t* message, uint32_t messageLength, Command& com) {
		constexpr uint32_t nonceSize = sizeof(sendMessagesCounter);
		uint8_t ad[sizeof(sendMessagesCounter)];
		*(decltype(sendMessagesCounter)*)ad = htonl(com.flags);
		sendMessagesCounter++;
		
		uint8_t nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES];
		memset(nonce, 0, sizeof(nonce));
		*(decltype(sendMessagesCounter)*)nonce
			= htonl(sendMessagesCounter);
		*(decltype(sendMessagesCounter)*)(cipher)
			= htonl(sendMessagesCounter);
		
		crypto_aead_chacha20poly1305_ietf_encrypt_detached(
				cipher + nonceSize,
				cipher + nonceSize + com.binaryData.size(), nullptr,
				com.binaryData.data(), com.binaryData.size(),
				ad, sizeof(ad),
				nullptr,
				nonce, sendingKey);
	}
	
	void ConnectionEncryptionState::DecryptMessage(
			std::vector<uint8_t>& receivedData, uint8_t* cipher,
			uint32_t size, uint32_t flags) {
		constexpr uint32_t nonceSize = sizeof(sendMessagesCounter);
		constexpr uint32_t macSize
			= crypto_aead_chacha20poly1305_ietf_ABYTES;
		uint8_t nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES];
		memset(nonce, 0, sizeof(nonce));
		memcpy(nonce, cipher, nonceSize);
		
		uint8_t ad[sizeof(sendMessagesCounter)];
		*(decltype(sendMessagesCounter)*)ad = htonl(flags);
		
		receivedData.resize(size-nonceSize
				-crypto_aead_chacha20poly1305_ietf_ABYTES);
		
		if(crypto_aead_chacha20poly1305_ietf_decrypt_detached(
				receivedData.data(), nullptr,
				cipher+nonceSize,
				receivedData.size(),
				cipher+size-macSize,
				ad, sizeof(ad),
				nonce,
				receivingKey)) {
			DEBUG("Failed to decrypt `%s`", receivedData.data());
			state = STATE_FAILED_TO_VERIFY_MESSAGE;
			enet_peer_disconnect(GetPeer()->peer, 2);
		}
	}
	
	
	void ConnectionEncryptionState::StartHandshake() {
		// TODO: send certificate instead of only public key

		sendMessagesCounter = 0;
		ENetPacket* packet = enet_packet_create(
				GetHost()->certKey->GetPublicKey(),
				crypto::SIGN_PUBLIC_KEY_BYTES,
				ENET_PACKET_FLAG_RELIABLE);
		enet_peer_send(GetPeer()->peer, 0, packet);
		state = STATE_SENT_CERT;
	}
	
	void ConnectionEncryptionState::ReceivedWhenStateSentCert(uint8_t* _data,
			uint32_t size, uint32_t flags) {
		if(size != crypto::SIGN_PUBLIC_KEY_BYTES) {
			DEBUG("Received size of certificate (public key) is invalid");
			state = STATE_FAILED_TO_AUTHENTICATE;
			enet_peer_disconnect(GetPeer()->peer, 1);
			return;
		}
		memcpy(peerPublicKey, _data, size);
		state = STATE_SENT_AND_RECEIVED_CERT;
		
		ENetPacket* packet = enet_packet_create(nullptr,
				crypto::KEX_PUBLIC_KEY_BYTES + crypto::SIGNATURE_BYTES,
				ENET_PACKET_FLAG_RELIABLE);
			
		crypto_kx_keypair(publicKey, secretKey);
		memcpy(packet->data, publicKey, crypto::KEX_PUBLIC_KEY_BYTES);
		GetHost()->certKey->Sign(packet->data+crypto::KEX_PUBLIC_KEY_BYTES,
				packet->data, crypto::KEX_PUBLIC_KEY_BYTES);
		enet_peer_send(GetPeer()->peer, 0, packet);
		state = STATE_SENT_KEX;
	}
	
	
	void ConnectionEncryptionState::ReceivedWhenStateSentKex(uint8_t* data,
			uint32_t size, uint32_t flags) {
		if(size != crypto::KEX_PUBLIC_KEY_BYTES + crypto::SIGNATURE_BYTES) {
			DEBUG("Received kex size is invalid");
			state = STATE_FAILED_TO_AUTHENTICATE;
			enet_peer_disconnect(GetPeer()->peer, 1);
			return;
		}
		if(crypto_sign_ed25519_verify_detached(
					(uint8_t*)data+crypto::KEX_PUBLIC_KEY_BYTES,
					(uint8_t*)data, crypto::KEX_PUBLIC_KEY_BYTES,
					peerPublicKey)) {
			DEBUG("Failed to verify kex message");
			state = STATE_FAILED_TO_AUTHENTICATE;
			enet_peer_disconnect(GetPeer()->peer, 2);
			return;
		}
		state = STATE_SENT_AND_RECEIVED_KEX;
		
		uint8_t *rx = receivingKey;
		uint8_t *tx = sendingKey;
		uint8_t *clientPublicKey = (uint8_t*)data;
		if(memcmp(publicKey, clientPublicKey,
					crypto::KEX_PUBLIC_KEY_BYTES) < 0) {
			if(crypto_kx_client_session_keys(rx, tx, publicKey,
						secretKey, clientPublicKey)) {
				DEBUG("Failed to generate client session key");
				state = STATE_FAILED_TO_AUTHENTICATE;
				enet_peer_disconnect(GetPeer()->peer, 2);
				return;
			}
		} else {
			if(crypto_kx_server_session_keys(rx, tx, publicKey,
						secretKey, clientPublicKey)) {
				DEBUG("Failed to generate server session key");
				state = STATE_FAILED_TO_AUTHENTICATE;
				enet_peer_disconnect(GetPeer()->peer, 2);
				return;
			}
		}
	
		state = STATE_BEFORE_ON_CONNECT_CALLBACK;
	}
	
	
	
	std::shared_ptr<Host> ConnectionEncryptionState::GetHost() {
		return GetPeer()->host;
	}
}

