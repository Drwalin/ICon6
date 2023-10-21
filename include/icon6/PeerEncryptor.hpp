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

#ifndef ICON6_PEER_ENCRYPTOR_HPP
#define ICON6_PEER_ENCRYPTOR_HPP

#include <cinttypes>

#include <memory>

#include <enet/enet.h>

#include "Cert.hpp"
#include "Flags.hpp"

namespace icon6
{
class PeerEncryptor
{
public:
	PeerEncryptor();
	~PeerEncryptor();

	ENetPacket *CreateHandshakePacketData(crypto::CertKey *certKey);
	ENetPacket *CreateKEXPacket(crypto::CertKey *certKey,
								ENetPacket *receivedHandshake);
	bool ReceiveKEX(ENetPacket *receivedKEX);

	static constexpr uint32_t GetEncryptedMessageOverhead()
	{
		return sizeof(sendMessagesCounter) +
			   crypto_aead_chacha20poly1305_ietf_ABYTES; // mac bytes
	}
	inline static uint32_t GetEncryptedMessageLength(uint32_t rawMessageLength)
	{
		return rawMessageLength + GetEncryptedMessageOverhead();
	}

	void EncryptMessage(uint8_t *cipher, const uint8_t *message,
						uint32_t messageLength, Flags flags);
	bool DecryptMessage(std::vector<uint8_t> &receivedData, uint8_t *cipher,
						uint32_t cipherLength, Flags flags);

	friend class ConnectionEncryptionState;

private:
	crypto::KexKeys keys;

	uint32_t sendMessagesCounter;
};
} // namespace icon6

#endif
