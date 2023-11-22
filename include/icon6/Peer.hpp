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

#include <cinttypes>

#include <vector>
#include <queue>

#include "Flags.hpp"
#include "ByteReader.hpp"

namespace icon6
{
class Host;

class Peer
{
public:
	virtual ~Peer();

	void Send(std::vector<uint8_t> &&data, Flags flags);

	void Disconnect();

	static constexpr uint32_t GetMaxSinglePacketMessagePayload() { return 1200; }
	virtual uint32_t GetRoundTripTime() const = 0;

	inline bool IsReadyToUse() const { return readyToUse; }

	inline Host *GetHost() { return host; }

	void SetReceiveCallback(void (*callback)(Peer *, ByteReader &,
											 Flags flags));
	void SetDisconnect(void (*callback)(Peer *));

public:
	uint64_t userData;
	void *userPointer;

	friend class Host;

	// thread unsafe
	virtual bool _InternalSend(const void *data, uint32_t length, Flags flags) = 0;
	// thread unsafe
	void _InternalSendOrQueue(std::vector<uint8_t> &data, Flags flags);
	// thread unsafe
	virtual void _InternalDisconnect() = 0;

protected:
	void SetReadyToUse();
	
	void CallCallbackDisconnect();

	struct SendCommand {
		std::vector<uint8_t> data;
		Flags flags;
	};

	void _InternalFlushQueuedSends();

protected:
	Peer(Host *host);

protected:
	std::queue<SendCommand> queuedSends;

	Host *host;

	void (*callbackOnReceive)(Peer *, ByteReader &, Flags);
	void (*callbackOnDisconnect)(Peer *);

	bool readyToUse;
};
} // namespace icon6

#endif
