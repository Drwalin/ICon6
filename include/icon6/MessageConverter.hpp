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
#include <bitscpp/ByteWriterExtensions.hpp>

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

class _InternalReader
{
public:
	static void ReadType(Peer *peer, Flags flags,
						 bitscpp::ByteReader<true> &reader, Flags &value)
	{
		value = flags;
	}

	static void ReadType(Peer *peer, Flags flags,
						 bitscpp::ByteReader<true> &reader,
						 std::shared_ptr<Host> &value)
	{
		value = peer->GetHost();
	}

	static void ReadType(Peer *peer, Flags flags,
						 bitscpp::ByteReader<true> &reader, Host *&value)
	{
		value = peer->GetHost().get();
	}

	static void ReadType(Peer *peer, Flags flags,
						 bitscpp::ByteReader<true> &reader,
						 std::shared_ptr<Peer> &value)
	{
		value = peer->shared_from_this();
		;
	}

	static void ReadType(Peer *peer, Flags flags,
						 bitscpp::ByteReader<true> &reader, Peer *&value)
	{
		value = peer;
	}

	template <typename T>
	static void ReadType(Peer *peer, Flags flags,
						 bitscpp::ByteReader<true> &reader, T &value)
	{
		reader.op(value);
	}
};

template <typename Tret> class MessageReturnExecutor
{
public:
	template <typename TF, typename Tuple, size_t... SeqArgs>
	static void Execute(TF &&onReceive, Tuple &args, Peer *peer, Flags flags,
						bitscpp::ByteReader<true> &reader,
						std::index_sequence<SeqArgs...>)
	{
		Tret ret = onReceive(std::get<SeqArgs>(args)...);
		if constexpr (!std::is_same<void, Tret>::value) {
			if (reader.get_buffer()[0] == 2 &&
				reader.get_remaining_bytes() == 4) {
				uint32_t id;
				reader.op(id);
				std::vector<uint8_t> buffer;
				bitscpp::ByteWriter writer(buffer);
				writer.op("_ret");
				writer.op(id);
				writer.op(ret);
				peer->Send(std::move(buffer), flags);
			}
		}
	}
};

template <> class MessageReturnExecutor<void>
{
public:
	template <typename TF, typename Tuple, size_t... SeqArgs>
	static void Execute(TF &&onReceive, Tuple &args, Peer *peer, Flags flags,
						bitscpp::ByteReader<true> &reader,
						std::index_sequence<SeqArgs...>)
	{
		onReceive(std::get<SeqArgs>(args)...);
	}
};

template <typename Tret, typename... Targs>
class MessageConverterSpec : public MessageConverter
{
public:
	using TupleType = std::tuple<typename std::remove_const<
		typename std::remove_reference<Targs>::type>::type...>;

	MessageConverterSpec(Tret (*onReceive)(Targs... args))
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
					   std::index_sequence<SeqArgs...> seq)
	{
		TupleType args;
		(_InternalReader::ReadType(peer, flags, reader,
								   std::get<SeqArgs>(args)),
		 ...);
		MessageReturnExecutor<Tret>::Execute(onReceive, args, peer, flags,
											 reader, seq);
	}

private:
	Tret (*const onReceive)(Targs...);
};

} // namespace icon6

#endif
