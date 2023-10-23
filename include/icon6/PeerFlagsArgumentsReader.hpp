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

#ifndef ICON6_PEER_FLAGS_ARGUMENTS_READER_HPP
#define ICON6_PEER_FLAGS_ARGUMENTS_READER_HPP

#include "ByteReader.hpp"
#include "Host.hpp"
#include "Peer.hpp"

namespace icon6
{

class PeerFlagsArgumentsReader
{
public:
	inline static void ReadType(Peer *peer, Flags flags, ByteReader &reader,
								Flags &value)
	{
		value = flags;
	}

	inline static void ReadType(Peer *peer, Flags flags, ByteReader &reader,
								Host *&value)
	{
		value = peer->GetHost();
	}

	inline static void ReadType(Peer *peer, Flags flags, ByteReader &reader,
								Peer *&value)
	{
		value = peer;
	}

	template <typename T>
	inline static void ReadType(Peer *peer, Flags flags, ByteReader &reader,
								T &value)
	{
		reader.op(value);
	}
};
} // namespace icon6

#endif
