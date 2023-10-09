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

#ifndef ICON6_MESSAGE_PASSING_ENVIRONMENT_HPP
#define ICON6_MESSAGE_PASSING_ENVIRONMENT_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <bitscpp/ByteWriterExtensions.hpp>
#include <bitscpp/ByteReaderExtensions.hpp>

#include "Host.hpp"
#include "Peer.hpp"

namespace icon6 {
	
	class MessagePassingEnvironment;
	
	class MessageConverter {
	public:
		virtual ~MessageConverter() = default;
		
		virtual void Call(MessagePassingEnvironment& mpe,
				Peer* peer, bitscpp::ByteReader<true>& reader,
				uint32_t flags) = 0;
	};
	
	template<typename T>
	class MessageConverterSpec : public MessageConverter {
	public:
		MessageConverterSpec(void(*onReceive)(Peer* peer, T&& message,
					uint32_t flags)) : onReceive(onReceive) {
		}
		
		virtual ~MessageConverterSpec() = default;
		
		virtual void Call(MessagePassingEnvironment& mpe,
				Peer* peer, bitscpp::ByteReader<true>& reader,
				uint32_t flags) override {
			T message;
			reader.op(message);
			onReceive(peer, std::move(message), flags);
		}
		
	private:
		
		void(*const onReceive)(Peer* peer, T&& message, uint32_t flags);
	};
	
	
	
	class MessagePassingEnvironment :
		public std::enable_shared_from_this<MessagePassingEnvironment> {
	public:
		
		using ReceiveCallback = void(*)(Peer* peer, void* message, uint32_t flags);
		
		virtual void OnReceive(Peer* peer, std::vector<uint8_t>& data,
				uint32_t flags);
		
		template<typename T>
		void RegisterMessage(const std::string& name,
				void(*onReceive)(Peer* peer, T&& message, uint32_t flags)) {
			registeredMessages[name]
				= std::make_shared<MessageConverterSpec<T>>(onReceive);
		}
		
		template<typename T>
		void Send(Peer* peer, const std::string& name, const T& message,
				uint32_t flags) {
			std::vector<uint8_t> buffer;
			{
				bitscpp::ByteWriter writer(buffer);
				writer.op(name);
				writer.op(message);
			}
			peer->Send(std::move(buffer), flags);
		}
		
	protected:
		
		std::unordered_map<std::string, std::shared_ptr<MessageConverter>>
			registeredMessages;
	};
}

#endif

