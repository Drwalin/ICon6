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

#ifndef ICON6_MESSAGE_CONVERTER_HPP
#define ICON6_MESSAGE_CONVERTER_HPP

#include <memory>
#include <tuple>

#include <bitscpp/ByteReaderExtensions.hpp>

#include "Host.hpp"
#include "Peer.hpp"

namespace icon6
{

class MessagePassingEnvironment;
class CommandExecutionQueue;

class MessageConverter
{
public:
	virtual ~MessageConverter() = default;

	virtual void Call(Peer *peer, bitscpp::ByteReader<true> &reader,
					  Flags flags) = 0;

	std::shared_ptr<CommandExecutionQueue> executionQueue;
};

template <typename... Targs>
class MessageConverterSpec : public MessageConverter
{
public:
	using TupleType = std::tuple<typename std::remove_const<
		typename std::remove_reference<Targs>::type>::type...>;

	MessageConverterSpec(void (*onReceive)(Flags flags, Targs... args))
		: onReceive(onReceive)
	{
	}

	virtual ~MessageConverterSpec() = default;

	virtual void Call(Peer *peer, bitscpp::ByteReader<true> &reader,
					  Flags flags) override
	{
		auto seq = std::index_sequence_for<Targs...>{};
		_InternalCall(peer, flags, reader, seq);
	}

private:
	template <size_t... SeqArgs>
	void _InternalCall(Peer *peer, Flags flags,
					   bitscpp::ByteReader<true> &reader,
					   std::index_sequence<SeqArgs...>)
	{
		TupleType args;
		(_InternalReadArgumentType(peer, flags, reader,
								   std::get<SeqArgs>(args)),
		 ...);
		onReceive(flags, std::get<SeqArgs>(args)...);
	}

	void _InternalReadArgumentType(Peer *peer, Flags flags,
								   bitscpp::ByteReader<true> &reader,
								   std::shared_ptr<Host> &value)
	{
		value = peer->GetHost();
	}

	void _InternalReadArgumentType(Peer *peer, Flags flags,
								   bitscpp::ByteReader<true> &reader,
								   Host *&value)
	{
		value = peer->GetHost().get();
	}

	void _InternalReadArgumentType(Peer *peer, Flags flags,
								   bitscpp::ByteReader<true> &reader,
								   std::shared_ptr<Peer> &value)
	{
		value = peer->shared_from_this();
		;
	}

	void _InternalReadArgumentType(Peer *peer, Flags flags,
								   bitscpp::ByteReader<true> &reader,
								   Peer *&value)
	{
		value = peer;
	}

	template <typename T>
	void _InternalReadArgumentType(Peer *peer, Flags flags,
								   bitscpp::ByteReader<true> &reader, T &value)
	{
		reader.op(value);
	}

private:
	void (*const onReceive)(Flags flags, Targs...);
};

} // namespace icon6

#endif
