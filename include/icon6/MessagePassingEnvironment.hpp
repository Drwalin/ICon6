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

#include <string>
#include <unordered_map>

#include "Host.hpp"
#include "Peer.hpp"
#include "Serializer.hpp"

namespace icon6 {
	
	struct MessageConverter {
		template<typename T>
		static MessageConverter Construct(
				void (*deserialize)(const uint8_t*, uint32_t, T*),
				void (*serialize)(std::vector<uint8_t>& data, const T*),
				void(*onReceive)(Peer& peer, T& message)
			) {
			return {deserialize, serialize, onReceive, sizeof(T)};
		}
		
		void (*const deserialize)(const uint8_t *data, uint32_t size, void* value);
		void (*const serialize)(std::vector<uint8_t>& data, const void* value);
		void(*onReceive)(Peer& peer, class XXX& message);
		const size_t size;
	};

	class MessagePassingEnvironment {
	public:
		
		MessagePassingEnvironment(std::shared_ptr<Host> host);
		
		template<typename T>
		void RegisterMessage(std::string name,
				void(*onReceive)(Peer& peer, T& message));
		
		template<typename T>
		void Send(std::shared_ptr<Peer> peer, const std::string& name, T& value,
				uint32_t flags);
		
		void OnReceive(const uint8_t* data, uint32_t size);
		
	private:
		
		std::shared_ptr<Host> host;
		
		std::unordered_map<std::string, MessageConverter> registeredMessages;
	};
	
	
	template<typename T>
	void MessagePassingEnvironment::RegisterMessage(std::string name,
			void(*onReceive)(Peer& peer, T& message)) {
		registeredMessages[name] = MessageConverter::Construct<T>(
				[](const uint8_t*data, uint32_t size, T*value) {
					Serializer s(data, size);
					value->__Deserialize(s);
				},
				[](std::vector<uint8_t>& data, const T*value) {
					Serializer s(data);
					value->__Serialize(s);
				},
				onReceive
			);
	}

	template<typename T>
	void MessagePassingEnvironment::Send(std::shared_ptr<Peer> peer,
			const std::string& name, T& value, uint32_t flags) {
		std::vector<uint8_t> data;
		Serializer s(data);
		value->__Serialize(s);
		peer->Send(std::move(data), flags);
	}
}

#endif

