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

#include <chrono>
#include <memory>

#include "../include/icon6/Peer.hpp"

#include "../include/icon6/Host.hpp"

namespace ICon6 {
	
	
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
		flags = 0;
		callbackOnConnect = nullptr;
		callbackOnReceive = nullptr;
		callbackOnDisconnect = nullptr;
		userData = nullptr;
		host = enet_host_create(address, 4095, 2, 0, 0);
	}
	
	void Host::Destroy() {
		WaitStop();
		enet_host_destroy(host);
		host = nullptr;
	}
	
	
	void Host::RunAsync() {
		flags = 0;
		std::thread([](std::shared_ptr<Host> host){
				host->RunSync();
			}, shared_from_this()).detach();
	}
	
	
	void Host::RunSync() {
		ENetEvent event;
		flags = RUNNING;
		while(enet_host_service(host, &event, 10) >= 0 && !(flags & TO_STOP)) {
			DispatchEvent(event);
			DispatchAllEventsFromQueue();
		}
		for(auto p : peers) {
			enet_peer_disconnect(p->peer, 0);
		}
		for(int i=0; i<1000000 && enet_host_service(host, &event, 20) >= 0;
				++i) {
			DispatchEvent(event);
			if(event.type == ENET_EVENT_TYPE_CONNECT) {
				enet_peer_disconnect(event.peer, 0);
			} else if(event.type == ENET_EVENT_TYPE_NONE && i>1000) {
				for(auto p : peers) {
					enet_peer_disconnect(p->peer, 0);
				}
			}
			if(peers.empty()) {
				break;
			}
		}
		flags &= ~RUNNING;
	}
	
	void Host::DispatchEvent(ENetEvent& event) {
		switch(event.type) {
			case ENET_EVENT_TYPE_CONNECT:
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
					Peer* peer = (Peer*)event.peer->data;
					peer->CallCallbackDisconnect(event.data);
					peers.erase(((Peer*)(event.peer->data))->shared_from_this());
				} break;
			default:
				;
		}
	}
	
	void Host::DispatchAllEventsFromQueue() {
		for(int i=0; i<10; ++i) {
			{
				std::lock_guard<std::mutex> lock(mutex);
				std::swap(poped_commands, enqueued_commands);
			}
			if(poped_commands.empty())
				return;
			for(Command& c : poped_commands) {
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
		while(flags & RUNNING) {
			Stop();
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}
	
	
	std::future<std::shared_ptr<Peer>> Host::Connect(std::string address,
			uint16_t port) {
		using PromiseType = std::promise<std::shared_ptr<Peer>>;
		std::shared_ptr<PromiseType> promise = std::make_shared<PromiseType>();
		std::thread([](std::string address, uint16_t port,
					std::shared_ptr<Host> host,
			std::shared_ptr<PromiseType> promise) {
				Command command;
				enet_address_set_host(&command.address, address.c_str());
				command.address.port = port;
				command.customData = promise;
				command.host = host;
				command.callback = [](Command&com){
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

