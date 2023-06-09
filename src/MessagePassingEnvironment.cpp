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

#include "../include/icon6/MessagePassingEnvironment.hpp"

namespace icon6 {
	void MessagePassingEnvironment::OnReceive(Peer* peer,
			const uint8_t* data, uint32_t size, uint32_t flags) {
		bitscpp::ByteReader reader(data, size);
		std::string name;
		reader.op(name);
		auto it = registeredMessages.find(name);
		if(registeredMessages.end() != it) {
			it->second->Call(peer, reader, flags);
		}
	}
}

