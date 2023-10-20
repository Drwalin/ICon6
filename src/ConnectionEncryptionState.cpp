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

namespace icon6
{
ConnectionEncryptionState::ConnectionEncryptionState() { state = STATE_NONE; }
ConnectionEncryptionState::~ConnectionEncryptionState() {}

void ConnectionEncryptionState::EncryptMessage(uint8_t *cipher,
											   const uint8_t *message,
											   uint32_t messageLength,
											   Flags flags)
{
	encryptor.EncryptMessage(cipher, message, messageLength, flags);
}

bool ConnectionEncryptionState::DecryptMessage(
	std::vector<uint8_t> &receivedData, uint8_t *cipher, uint32_t size,
	Flags flags)
{
	if (!encryptor.DecryptMessage(receivedData, cipher, size, flags)) {
		state = STATE_FAILED_TO_VERIFY_MESSAGE;
		return false;
	}
	return true;
}

void ConnectionEncryptionState::StartHandshake(Peer *peer)
{
	// TODO: send certificate instead of only public key
	encryptor.sendMessagesCounter = 0;
	ENetPacket *packet =
		encryptor.CreateHandshakePacketData(peer->host->certKey);
	enet_peer_send(peer->peer, 0, packet);
	state = STATE_SENT_CERT;
}

void ConnectionEncryptionState::ReceivedWhenStateSentCert(
	Peer *peer, ENetPacket *receivedPacket, Flags flags)
{
	ENetPacket *packet =
		encryptor.CreateKEXPacket(peer->host->certKey, receivedPacket);
	if (packet) {
		state = STATE_SENT_AND_RECEIVED_CERT;
		enet_peer_send(peer->peer, 0, packet);
		state = STATE_SENT_KEX;
	} else {
		state = STATE_FAILED_TO_AUTHENTICATE;
		enet_peer_disconnect(peer->peer, 1);
	}
}

void ConnectionEncryptionState::ReceivedWhenStateSentKex(Peer *peer,
														 ENetPacket *kex,
														 Flags flags)
{
	if (encryptor.ReceiveKEX(kex)) {
		state = STATE_BEFORE_ON_CONNECT_CALLBACK;
	} else {
		state = STATE_FAILED_TO_AUTHENTICATE;
		enet_peer_disconnect(peer->peer, 1);
	}
}
} // namespace icon6
