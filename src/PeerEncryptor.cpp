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

#include <cstring>
#include <cstddef>

#include "../include/icon6/Host.hpp"
#include "../include/icon6/Cert.hpp"

#include "../include/icon6/PeerEncryptor.hpp"

namespace icon6
{
PeerEncryptor::PeerEncryptor() { sodium_mlock(this, sizeof(*this)); }
PeerEncryptor::~PeerEncryptor()
{
	sodium_memzero(this, sizeof(*this));
	sodium_munlock(this, sizeof(*this));
}

void PeerEncryptor::EncryptMessage(uint8_t *cipher, const uint8_t *message,
								   uint32_t messageLength, Flags flags)
{
	constexpr uint32_t nonceSize = sizeof(sendMessagesCounter);
	uint8_t ad[sizeof(flags.field)];
	flags.GetNetworkOrder(ad);
	sendMessagesCounter++;

	uint8_t *ptrCipher = cipher;
	uint8_t *ptrNonce = ptrCipher + messageLength;
	uint8_t *ptrMac = ptrNonce + nonceSize;

	uint8_t nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES];
	memset(nonce, 0, sizeof(nonce));
	
	{
		uint32_t v = htonl(sendMessagesCounter);
		memcpy(ptrNonce, &v, sizeof(uint32_t));
		memcpy(nonce, &v, sizeof(uint32_t));
	}

	const uint8_t *ptrMessage = message;

	crypto_aead_chacha20poly1305_ietf_encrypt_detached(
		ptrCipher, ptrMac, nullptr, ptrMessage, messageLength, ad, sizeof(ad),
		nullptr, nonce, keys.sendingKey);
}

bool PeerEncryptor::DecryptMessage(std::vector<uint8_t> &receivedData,
								   uint8_t *cipher, uint32_t size, Flags flags)
{
	constexpr uint32_t nonceSize = sizeof(sendMessagesCounter);
	uint8_t nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES];
	memset(nonce, 0, sizeof(nonce));

	uint8_t ad[sizeof(flags.field)];
	flags.GetNetworkOrder(ad);

	const uint32_t messageLength = size - GetEncryptedMessageOverhead();
	receivedData.resize(messageLength);

	const uint8_t *ptrCipher = cipher;
	const uint8_t *ptrNonce = ptrCipher + messageLength;
	const uint8_t *ptrMac = ptrNonce + nonceSize;
	memcpy(nonce, ptrNonce, nonceSize);

	uint8_t *ptrMessage = receivedData.data();

	if (crypto_aead_chacha20poly1305_ietf_decrypt_detached(
			ptrMessage, nullptr, ptrCipher, messageLength, ptrMac, ad,
			sizeof(ad), nonce, keys.receivingKey)) {
		DEBUG("Failed to decrypt `%s`", receivedData.data());
		return false;
	}
	return true;
}

ENetPacket *PeerEncryptor::CreateHandshakePacketData(crypto::CertKey *certKey)
{
	return enet_packet_create(certKey->GetPublicKey(),
							  crypto::SIGN_PUBLIC_KEY_BYTES,
							  ENET_PACKET_FLAG_RELIABLE);
}

ENetPacket *PeerEncryptor::CreateKEXPacket(crypto::CertKey *certKey,
										   ENetPacket *receivedHandshake)
{
	if (receivedHandshake->dataLength != crypto::SIGN_PUBLIC_KEY_BYTES) {
		DEBUG("Received size of certificate (public key) is invalid");
		return nullptr;
	}
	memcpy(keys.peerPublicKey, receivedHandshake->data,
		   receivedHandshake->dataLength);

	ENetPacket *packet = enet_packet_create(
		nullptr, crypto::KEX_PUBLIC_KEY_BYTES + crypto::SIGNATURE_BYTES,
		ENET_PACKET_FLAG_RELIABLE);

	crypto_kx_keypair(keys.publicKey, keys.secretKey);
	memcpy(packet->data, keys.publicKey, crypto::KEX_PUBLIC_KEY_BYTES);
	certKey->Sign(packet->data + crypto::KEX_PUBLIC_KEY_BYTES, packet->data,
				  crypto::KEX_PUBLIC_KEY_BYTES);
	return packet;
}

bool PeerEncryptor::ReceiveKEX(ENetPacket *kex)
{
	if (kex->dataLength !=
		crypto::KEX_PUBLIC_KEY_BYTES + crypto::SIGNATURE_BYTES) {
		DEBUG("Received kex size is invalid");
		return false;
	}
	if (crypto_sign_ed25519_verify_detached(
			kex->data + crypto::KEX_PUBLIC_KEY_BYTES, kex->data,
			crypto::KEX_PUBLIC_KEY_BYTES, keys.peerPublicKey)) {
		DEBUG("Failed to verify kex message");
		return false;
	}

	uint8_t *rx = keys.receivingKey;
	uint8_t *tx = keys.sendingKey;
	uint8_t *clientPublicKey = kex->data;
	if (memcmp(keys.publicKey, clientPublicKey, crypto::KEX_PUBLIC_KEY_BYTES) <
		0) {
		if (crypto_kx_client_session_keys(rx, tx, keys.publicKey,
										  keys.secretKey, clientPublicKey)) {
			DEBUG("Failed to generate client session key");
			return false;
		}
	} else {
		if (crypto_kx_server_session_keys(rx, tx, keys.publicKey,
										  keys.secretKey, clientPublicKey)) {
			DEBUG("Failed to generate server session key");
			return false;
		}
	}

	return true;
}
} // namespace icon6
