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
#include <memory>
#include <future>
#include <atomic>
#include <unordered_set>
#include <vector>

#include "Peer.hpp"
#include "Command.hpp"

#define DEBUG(...) icon6::Debug(__FILE__,__LINE__,__VA_ARGS__)

namespace icon6 {
	void Debug(const char*file, int line,const char*fmt, ...);
	
	class Peer;
	
	class Host final : public std::enable_shared_from_this<Host> {
	public:
		
		static std::shared_ptr<Host> Make(uint16_t port,
				uint32_t maximumHostsNumber);
		
		// create host on 127.0.0.1:port
		Host(uint16_t port, uint32_t maximumHostsNumber);
		
		// create client host on any port
		Host(uint32_t maximumHostsNumber);
		~Host();
		
		void Destroy();
		
		void RunAsync();
		void RunSync();
		
		void SetConnect(void(*callback)(Peer*));
		void SetReceive(void(*callback)(Peer*, void* data, uint32_t size,
					uint32_t flags));
		void SetDisconnect(void(*callback)(Peer*, uint32_t disconnectData));
		
		void Stop();
		void WaitStop();
		
	public:
		// thread safe function to connect to a remote host
		std::future<std::shared_ptr<Peer>> Connect(std::string address,
				uint16_t port);
		
	public:
		
		void* userData;
		std::shared_ptr<void> userSharedPointer;
		
		friend class Peer;
		
	private:
		
		void Init(ENetAddress* address, uint32_t maximumHostsNumber);
		void DispatchEvent(ENetEvent& event);
		void DispatchAllEventsFromQueue();
		void DispatchPopedEventsFromQueue();
		void EnqueueCommand(Command&& command);
		
	private:
		
		enum HostFlags : uint32_t {
			RUNNING = 1<<0,
			TO_STOP = 1<<1,
		};
		
		ENetHost* host;
		std::atomic<uint32_t> flags;
		
		std::unordered_set<std::shared_ptr<Peer>> peers;
	
		union {
			void *__concurrentQueueVoid;
#ifdef ICON6_HOST_CPP_IMPLEMENTATION
			moodycamel::ConcurrentQueue<Command> *concurrentQueueCommands;
#endif
		};
		std::vector<Command> poped_commands;
		
		void(*callbackOnConnect)(Peer*);
		void(*callbackOnReceive)(Peer*, void* data, uint32_t size,
					uint32_t flags);
		void(*callbackOnDisconnect)(Peer*, uint32_t disconnectData);
	};
	
	uint32_t Initialize();
	void Deinitialize();
}

#endif
