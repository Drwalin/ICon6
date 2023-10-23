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

#ifndef ICON6_METHOD_INVOCATION_CONVERTER_HPP
#define ICON6_METHOD_INVOCATION_CONVERTER_HPP

#include <string>
#include <unordered_map>
#include <vector>

#include <bitscpp/ByteWriterExtensions.hpp>
#include <bitscpp/ByteReaderExtensions.hpp>

#include "PeerFlagsArgumentsReader.hpp"
#include "Host.hpp"
#include "Peer.hpp"
#include "MessageConverter.hpp"

namespace icon6
{

class CommandExecutionQueue;

namespace rmi
{

class Class;

class MethodInvocationConverter
{
public:
	virtual ~MethodInvocationConverter() = default;

	virtual void Call(void *objectPtr, Peer *peer, ByteReader &reader,
					  Flags flags) = 0;

	CommandExecutionQueue *executionQueue;
};

template <typename Tret> class InvocationReturnExecutor
{
public:
	template <typename TClass, typename TF, typename Tuple, size_t... SeqArgs>
	static void Execute(TClass *objectPtr, TF &&onReceive, Tuple &args,
						Peer *peer, Flags flags, ByteReader &reader,
						std::index_sequence<SeqArgs...>)
	{
		Tret ret = (objectPtr->*onReceive)(std::get<SeqArgs>(args)...);
		if (reader.bytes[0] == MethodProtocolSendFlags::METHOD_CALL_PREFIX &&
			reader.get_remaining_bytes() == 4) {
			uint32_t id;
			reader.op(id);
			std::vector<uint8_t> buffer;
			bitscpp::ByteWriter writer(buffer);
			writer.op(MethodProtocolSendFlags::RETURN_CALLBACK);
			writer.op(id);
			writer.op(ret);
			peer->Send(std::move(buffer), flags);
		}
	}
};

template <> class InvocationReturnExecutor<void>
{
public:
	template <typename TClass, typename TF, typename Tuple, size_t... SeqArgs>
	static void Execute(TClass *objectPtr, TF &&onReceive, Tuple &args,
						Peer *peer, Flags flags, ByteReader &reader,
						std::index_sequence<SeqArgs...>)
	{
		(objectPtr->*onReceive)(std::get<SeqArgs>(args)...);
		if (reader.bytes[0] == MethodProtocolSendFlags::METHOD_CALL_PREFIX &&
			reader.get_remaining_bytes() == 4) {
			uint32_t id;
			reader.op(id);
			std::vector<uint8_t> buffer;
			bitscpp::ByteWriter writer(buffer);
			writer.op(MethodProtocolSendFlags::RETURN_CALLBACK);
			writer.op(id);
			peer->Send(std::move(buffer), flags);
		}
	}
};

template <typename Tclass, typename Tret, typename... Targs>
class MessageNetworkAwareMethodInvocationConverterSpec
	: public MethodInvocationConverter
{
public:
	using TupleType = std::tuple<typename std::remove_const<
		typename std::remove_reference<Targs>::type>::type...>;

	MessageNetworkAwareMethodInvocationConverterSpec(
		Class *_class, Tret (Tclass::*memberFunction)(Targs... args))
		: onReceive(memberFunction), _class(_class)
	{
	}

	virtual ~MessageNetworkAwareMethodInvocationConverterSpec()
	{
		_class = nullptr;
		onReceive = nullptr;
	}

	virtual void Call(void *objectPtr, Peer *peer, ByteReader &reader,
					  Flags flags) override
	{
		Tclass *ptr = (Tclass *)objectPtr;
		auto seq = std::index_sequence_for<Targs...>{};
		_InternalCall(ptr, peer, flags, reader, seq);
	}

private:
	template <size_t... SeqArgs>
	void _InternalCall(Tclass *ptr, Peer *peer, Flags flags, ByteReader &reader,
					   std::index_sequence<SeqArgs...> seq)
	{
		TupleType args;
		(PeerFlagsArgumentsReader::ReadType(peer, flags, reader,
											std::get<SeqArgs>(args)),
		 ...);
		InvocationReturnExecutor<Tret>::Execute(ptr, onReceive, args, peer,
												flags, reader, seq);
	}

private:
	Tret (Tclass::*onReceive)(Targs... args);
	class Class *_class;
};

} // namespace rmi
} // namespace icon6

#endif
