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

#ifndef ICON6_HOST_HPP
#define ICON6_HOST_HPP

#include <enet/enet.h>

#include <string>
#include <thread>
#include <memory>

namespace ICon6 {
	
	enum MessageFlags {
		SEQUENCED = ENET_PACKET_FLAG_RELIABLE,
		UNSEQUENCED = ENET_PACKET_FLAG_RELIABLE | ENET_PACKET_FLAG_UNSEQUENCED,
		UNRELIABLE = 0
	};
	
	class Peer;
	
	class Host {
	public:
		
		Host();
		~Host();
		
		void RunAsync();
		
		void OnConnect(void(*callback)(Peer*, uint32_t data));
		void OnReceive(void(*callback)(Peer*, void* data, uint32_t size,
					MessageFlags flags));
		void OnDisconnect(void(*callback)(Peer*, uint32_t data));
		
	public:
		// thread secure
		
		std::shared_ptr<Peer> Connect(std::string ipAddress, uint16_t port);
		
	private:
		
		ENetHost* host;
		
		
		
	};
	
	void Initialize();
	void Deinitialize();
}

#endif

