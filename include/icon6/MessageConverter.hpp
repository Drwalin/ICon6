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
					  uint32_t flags) = 0;

	std::shared_ptr<CommandExecutionQueue> executionQueue;
};

template <typename T> class MessageConverterSpec : public MessageConverter
{
  public:
	MessageConverterSpec(void (*onReceive)(Peer *peer, uint32_t flags, T message))
		: onReceive(onReceive)
	{
	}

	virtual ~MessageConverterSpec() = default;

	virtual void Call(Peer *peer, bitscpp::ByteReader<true> &reader,
					  uint32_t flags) override
	{
		typename std::remove_const<
			typename std::remove_reference<T>::type>::type message;
		reader.op(message);
		if constexpr (std::is_rvalue_reference<T>::value)
			onReceive(peer, flags, std::move(message));
		else
			onReceive(peer, flags, message);
	}

  private:
	void (*const onReceive)(Peer *peer, uint32_t flags, T message);
};

} // namespace icon6

#endif
