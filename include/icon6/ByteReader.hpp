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

#ifndef ICON6_BYTE_READER_HPP
#define ICON6_BYTE_READER_HPP

#include "../../bitscpp/include/bitscpp/ByteReaderExtensions.hpp"

namespace icon6
{
class ByteReader : public bitscpp::ByteReader<true>
{
public:
	union {
#ifdef ISTEAMNETWORKINGSOCKETS
		ISteamNetworkingMessage *packet;
#endif
		void *_packet;
	};

public:
#ifdef ISTEAMNETWORKINGSOCKETS
	ByteReader(ISteamNetworkingMessage *packet, uint32_t offset)
		: bitscpp::ByteReader<true>((uint8_t *)packet->m_pData, offset,
									packet->m_cbSize),
		  packet(packet)
	{
	}
#endif
	~ByteReader();

	ByteReader(ByteReader &&o)
		: bitscpp::ByteReader<true>(o.buffer, o.offset, o.size),
		  _packet(o._packet)
	{
		errorReading_bufferToSmall = o.errorReading_bufferToSmall;
		o._packet = nullptr;
	}

	inline ByteReader &operator=(ByteReader &&o)
	{
		this->~ByteReader();
		new (this) ByteReader(std::move(o));
		return *this;
	}

	inline uint8_t const *data() { return buffer; }

	ByteReader(ByteReader &) = delete;
	ByteReader(const ByteReader &) = delete;
	ByteReader &operator=(ByteReader &) = delete;
	ByteReader &operator=(const ByteReader &) = delete;
};
} // namespace icon6

#endif
