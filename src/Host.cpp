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

#include <cstdarg>

#include <chrono>
#include <memory>
#include <exception>

#include "../include/icon6/Peer.hpp"

#include "../include/icon6/Host.hpp"



namespace icon6 {
	
	void Debug(const char*file, int line,const char*fmt, ...) {
		va_list va;
		va_start(va, fmt);
		static std::mutex mutex;
		std::lock_guard<std::mutex> lock(mutex);
		fprintf(stderr, "%s:%i\t\t", file, line);
		vfprintf(stderr, fmt, va);
		fprintf(stderr, "\n");
		fflush(stderr);
	}

	std::shared_ptr<Host> Host::Make(uint16_t port,
			uint32_t maximumHostsNumber) {
		return std::make_shared<Host>(port, maximumHostsNumber);
	}
	
	Host::Host(uint16_t port, uint32_t maximumHostsNumber) {
		ENetAddress addr;
		addr.host = ENET_HOST_ANY;
		addr.port = port;
		Init(&addr, maximumHostsNumber);
	}
	
	Host::Host(uint32_t maximumHostsNumber) {
		Init(nullptr, maximumHostsNumber);
	}
	
	Host::~Host() {
		Destroy();
	}
	
	
	void Host::Init(ENetAddress* address, uint32_t maximumHostsNumber) {
		DEBUG("");
		flags = 0;
		callbackOnConnect = nullptr;
		callbackOnReceive = nullptr;
		callbackOnDisconnect = nullptr;
		userData = nullptr;
		host = enet_host_create(address, maximumHostsNumber, 2, 0, 0);
		DEBUG("");
	}
	
	void Host::Destroy() {
		DEBUG("");
		WaitStop();
		enet_host_destroy(host);
		host = nullptr;
		DEBUG("");
	}
	
	
	void Host::RunAsync() {
		DEBUG("");
		flags = 0;
		std::thread([](std::shared_ptr<Host> host){
		DEBUG("");
				host->RunSync();
		DEBUG("");
			}, shared_from_this()).detach();
		DEBUG("");
	}
	
	
	void Host::RunSync() {
		DEBUG("");
		ENetEvent event;
		flags = RUNNING;
		DEBUG("");
		uint32_t err;
		while(!(flags & TO_STOP)) {
			err = enet_host_service(host, &event, 100);
		DEBUG("RUN SYNC LOOP %i ERR = %i", host->address.port, err);
			DispatchAllEventsFromQueue();
			DispatchEvent(event);
		}
		DEBUG("STARTING STOP ASYNC SEQUENCE OF HOST %i", host->address.port);
		for(auto p : peers) {
			enet_peer_disconnect(p->peer, 0);
		}
		for(int i=0; i<1000000 && enet_host_service(host, &event, 100) >= 0;
				++i) {
		DEBUG("");
			DispatchEvent(event);
		DEBUG("TRYING TO STOP HOST %i", host->address.port);
			if(event.type == ENET_EVENT_TYPE_CONNECT) {
				enet_peer_disconnect(event.peer, 0);
			} else if(event.type == ENET_EVENT_TYPE_NONE && i%10 == 0) {
				for(auto p : peers) {
					enet_peer_disconnect(p->peer, 0);
				}
			}
			fprintf(stderr, " peers.size() = %lu\n", peers.size());
			if(peers.empty()) {
				break;
			}
		}
		DEBUG("STOPPED HOST %i", host->address.port);
		flags &= ~RUNNING;
	}
	
	void Host::DispatchEvent(ENetEvent& event) {
		switch(event.type) {
			case ENET_EVENT_TYPE_CONNECT:
		DEBUG("%i EVENT CONNECT FROM %i\n\n\t\t\tCONNECT!!\n", host->address.port, event.peer->address.port);
				{
					if(event.peer == nullptr) {
						std::shared_ptr<Peer> peer(new Peer(shared_from_this(),
							event.peer));
						peers.insert(peer);
					}
					// TODO: instead of callback on connect do hand shake in
					//       receive
					if(callbackOnConnect) {
						callbackOnConnect((Peer*)event.peer->data);
					}
				} break;
			case ENET_EVENT_TYPE_RECEIVE:
				if(callbackOnReceive) {
		DEBUG("%i EVENT RECEIVE %i", host->address.port, event.peer->address.port);
					// TODO: first check for peer state and whether handshake is
					//       done and act accordingly
					uint32_t flags = 0;
					if(event.packet->flags & ENET_PACKET_FLAG_RELIABLE)
						flags &= FLAG_RELIABLE;
					if(!(event.packet->flags & ENET_PACKET_FLAG_UNSEQUENCED))
						flags &= FLAG_SEQUENCED;
					// TODO: decrypt here
					Peer* peer = (Peer*)event.peer->data;
					peer->CallCallbackReceive(event.packet->data,
							event.packet->dataLength, flags);
					enet_packet_destroy(event.packet);
				} break;
			case ENET_EVENT_TYPE_DISCONNECT:
				{
		DEBUG("%i EVENT RECEIVE %i", host->address.port, event.peer->address.port);
					Peer* peer = (Peer*)event.peer->data;
					peer->CallCallbackDisconnect(event.data);
					peers.erase(((Peer*)(event.peer->data))->shared_from_this());
				} break;
			default:
				DEBUG("EVENT NONE OF HOST %i", host->address.port);
				;
		}
	}
	
	void Host::DispatchAllEventsFromQueue() {
		for(int i=0; i<10; ++i) {
			{
				std::lock_guard<std::mutex> lock(mutex);
				std::swap(poped_commands, enqueued_commands);
			}
		DEBUG("DISPATCHING EVENTS OF HOST %i COUNT %i", host->address.port, poped_commands.size());
			if(poped_commands.empty())
				return;
			for(Command& c : poped_commands) {
		DEBUG("DISPATCHING EVENT FROM QUEUE");
				c.CallCallback();
				++i;
			}
			poped_commands.clear();
		}
	}
	
	void Host::SetConnect(void(*callback)(Peer*)) {
		callbackOnConnect = callback;
	}
	
	void Host::SetReceive(void(*callback)(Peer*, void* data, uint32_t size,
				uint32_t flags)) {
		callbackOnReceive = callback;
	}
	
	void Host::SetDisconnect(void(*callback)(Peer*, uint32_t disconnectData)) {
		callbackOnDisconnect = callback;
	}
	
	void Host::Stop() {
		flags |= TO_STOP;
	}
	
	void Host::WaitStop() {
		DEBUG("WAITING STOP HOST %i", host->address.port);
		while(flags & RUNNING) {
		DEBUG(" ... ... ... ... %i", host->address.port);
			Stop();
			std::this_thread::sleep_for(std::chrono::milliseconds(25));
		}
	}
	
	
	std::future<std::shared_ptr<Peer>> Host::Connect(std::string address,
			uint16_t port) {
		DEBUG("INIT CONNECT FROM HOST %i TO %i", host->address.port, port);
		using PromiseType = std::promise<std::shared_ptr<Peer>>;
		std::shared_ptr<PromiseType> promise = std::make_shared<PromiseType>();
		std::thread([](std::string address, uint16_t port,
					std::shared_ptr<Host> host,
			std::shared_ptr<PromiseType> promise) {
				std::shared_ptr<PromiseType> promise2
					= std::make_shared<PromiseType>();
				DEBUG("ASYNC FETCHING ADDRESS FOR CONNECTION FROM HOST %i TO %i", host->host->address.port, port);
				Command command;
				enet_address_set_host(&command.address, address.c_str());
				DEBUG("CONNECTION ADDRESS RECEIVED WHEN CONNECTING FROM HOST %i TO %i -> %8.8X", host->host->address.port, port, command.address.host);
				command.address.port = port;
				command.customData = promise2;
				command.host = host;
				command.callback = [](Command&com){
					DEBUG("EXECUTING CONNECT %i TO %i", com.host->host->address.port, com.address.port);
					ENetAddress addr;
					ENetPeer* peer
							= enet_host_connect(com.host->host, &addr, 2, 0);
					std::shared_ptr<Peer> p(new Peer(com.host, peer));
					peer->data = p.get();
					com.host->peers.insert(p);
					auto promise = std::static_pointer_cast<PromiseType>(
							com.customData);
					promise->set_value(p);
				};
				host->EnqueueCommand(std::move(command));
				auto future = promise2->get_future().share();
				future.wait();
				if(!future.valid()) {
					promise->set_value(nullptr);
					return;
				}
				auto peer = future.get();
				
				auto time_end = std::chrono::steady_clock::now()
					+ std::chrono::seconds(5);
				while(time_end > std::chrono::steady_clock::now()) {
					if(peer->state == Peer::STATE_READY_TO_USE) {
						promise->set_value(peer);
						return;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(4));
				}
				promise->set_value(nullptr);
				return;
			}, address, port, shared_from_this(), promise).detach();
		return promise->get_future();
	}
	
	void Host::EnqueueCommand(Command&& command) {
		std::lock_guard<std::mutex> lock(mutex);
		enqueued_commands.emplace_back(std::move(command));
	}
	
	
	
	
	
	uint32_t Initialize() {
		return enet_initialize();
	}
	
	void Deinitialize() {
		enet_deinitialize();
	}
}

