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

#ifndef ICON6_CONNECTION_ENCRYPTION_STATE_HPP
#define ICON6_CONNECTION_ENCRYPTION_STATE_HPP

#include <cinttypes>

#include <thread>
#include <memory>
#include <atomic>

#include "Flags.hpp"
#include "Cert.hpp"

namespace icon6
{

enum PeerConnectionState : uint32_t {
	STATE_NONE,

	STATE_ZOMBIE,
	STATE_DISCONNECTED,
	STATE_FAILED_TO_AUTHENTICATE,
	STATE_FAILED_TO_VERIFY_MESSAGE,

	STATE_SENT_CERT,
	STATE_SENT_AND_RECEIVED_CERT,

	STATE_SENT_KEX,
	STATE_SENT_AND_RECEIVED_KEX,

	STATE_BEFORE_ON_CONNECT_CALLBACK,
	STATE_READY_TO_USE,
};

class Peer;
class Command;
class Host;

class ConnectionEncryptionState
{
public:
	ConnectionEncryptionState();
	~ConnectionEncryptionState();

	static constexpr uint32_t GetEncryptedMessageOverhead()
	{
		return sizeof(sendMessagesCounter) +
			   crypto_aead_chacha20poly1305_ietf_ABYTES;
	}
	inline static uint32_t GetEncryptedMessageLength(uint32_t rawMessageLength)
	{
		return rawMessageLength + GetEncryptedMessageOverhead();
	}
	void EncryptMessage(uint8_t *cipher, const uint8_t *message,
						uint32_t messageLength, Flags flags);
	bool DecryptMessage(std::vector<uint8_t> &receivedData, uint8_t *cipher,
						uint32_t cipherLength, Flags flags);

	void StartHandshake(Peer *peer);
	void ReceivedWhenStateSentCert(Peer *peer, uint8_t *data, uint32_t size,
								   Flags flags);
	void ReceivedWhenStateSentKex(Peer *peer, uint8_t *data, uint32_t size,
								  Flags flags);

	inline PeerConnectionState GetState() const { return state; }

	friend class Peer;

protected:
	uint8_t secretKey[crypto::KEX_SECRET_KEY_BYTES];
	uint8_t publicKey[crypto::KEX_PUBLIC_KEY_BYTES];
	uint8_t sendingKey[32];
	uint8_t receivingKey[32];
	uint8_t peerPublicKey[crypto::SIGN_PUBLIC_KEY_BYTES];
	uint32_t sendMessagesCounter;

	PeerConnectionState state;
};
} // namespace icon6

#endif
