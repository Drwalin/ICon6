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
#include <thread>
#include <mutex>
#include <vector>

#include "../include/icon6/Peer.hpp"
#include "../include/icon6/MessagePassingEnvironment.hpp"

#include "../include/icon6/Host.hpp"

namespace icon6
{

void Debug(const char *file, int line, const char *fmt, ...)
{
	static std::atomic<int> globID = 1;
	thread_local static int id = globID++;

	va_list va;
	va_start(va, fmt);
	static std::mutex mutex;
	std::lock_guard<std::mutex> lock(mutex);
	fprintf(stderr, "%s:%i\t\t [ %2i ] \t ", file, line, id);
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
	fflush(stderr);
}

std::shared_ptr<Host> Host::Make(uint16_t port, uint32_t maximumHostsNumber)
{
	return std::make_shared<Host>(port, maximumHostsNumber);
}

Host::Host(uint16_t port, uint32_t maximumHostsNumber)
{
	ENetAddress addr;
	addr.host = ENET_HOST_ANY;
	addr.port = port;
	Init(&addr, maximumHostsNumber);
}

Host::Host(uint32_t maximumHostsNumber) { Init(nullptr, maximumHostsNumber); }

Host::~Host() { Destroy(); }

void Host::SetCertificatePolicy(PeerAcceptancePolicy peerAcceptancePolicy)
{
	if (peerAcceptancePolicy == PeerAcceptancePolicy::ACCEPT_ALL) {
		this->peerAcceptancePolicy = peerAcceptancePolicy;
	} else {
		throw "Please use only "
			  "Host::SetCertificatePolicy(PeerAcceptancePolicy::ACCEPT_ALL)";
	}
}

void Host::AddTrustedRootCA(std::shared_ptr<crypto::Cert> rootCA)
{
	throw "Host::SetSelfCertificate not implemented yet. Please use only "
		  "Host::SetCertificatePolicy(PeerAcceptancePolicy::ACCEPT_ALL)";
}

void Host::SetSelfCertificate(std::shared_ptr<crypto::Cert> root)
{
	throw "Host::SetSelfCertificate not implemented yet. Please use only "
		  "Host::InitRandomSelfsignedCertificate()";
}

void Host::InitRandomSelfsignedCertificate()
{
	certKey = crypto::CertKey::GenerateKey();
	// TODO: generate self signed certificate
}

void Host::Init(ENetAddress *address, uint32_t maximumHostsNumber)
{
	commandQueue = std::make_shared<CommandExecutionQueue>();
	flags = 0;
	callbackOnConnect = nullptr;
	callbackOnReceive = nullptr;
	callbackOnDisconnect = nullptr;
	userData = nullptr;
	host = enet_host_create(address, maximumHostsNumber, 2, 0, 0);
	SetCertificatePolicy(PeerAcceptancePolicy::ACCEPT_ALL);
	InitRandomSelfsignedCertificate();
}

void Host::Destroy()
{
	WaitStop();
	enet_host_destroy(host);
	host = nullptr;
}

void Host::RunAsync()
{
	flags = 0;
	std::thread([](std::shared_ptr<Host> host) { host->RunSync(); },
				shared_from_this())
		.detach();
}

void Host::RunSync()
{
	flags = RUNNING;
	while (flags & RUNNING) {
		RunSingleLoop(4);
	}
}

void Host::DisconnectAllGracefully()
{
	for (auto p : peers) {
		enet_peer_disconnect(p->peer, 0);
	}
	ENetEvent event;
	for (int i = 0; i < 1000000 && enet_host_service(host, &event, 10) >= 0;
		 ++i) {
		DispatchEvent(event);
		if (event.type == ENET_EVENT_TYPE_CONNECT) {
			enet_peer_disconnect(event.peer, 0);
		} else if (event.type == ENET_EVENT_TYPE_NONE && i % 10 == 0) {
			for (auto p : peers) {
				enet_peer_disconnect(p->peer, 0);
			}
		}
		if (peers.empty()) {
			break;
		}
	}
}

void Host::RunSingleLoop(uint32_t maxWaitTimeMilliseconds)
{
	if (flags & TO_STOP) {
		DisconnectAllGracefully();
		flags &= ~RUNNING;
	} else {
		flags |= RUNNING;
		ENetEvent event;
		while (!(flags & TO_STOP)) {
			enet_host_service(host, &event, maxWaitTimeMilliseconds);
			DispatchAllEventsFromQueue();
			DispatchEvent(event);
		}
	}
}

void Host::DispatchEvent(ENetEvent &event)
{
	switch (event.type) {
	case ENET_EVENT_TYPE_CONNECT: {
		if (event.peer->data == nullptr) {
			std::shared_ptr<Peer> peer(
				new Peer(shared_from_this(), event.peer));
			peers.insert(peer);
		}
		((Peer *)event.peer->data)->StartHandshake();
	} break;
	case ENET_EVENT_TYPE_RECEIVE: {
		uint32_t flags = 0;
		if (event.packet->flags & ENET_PACKET_FLAG_RELIABLE)
			flags &= FLAG_RELIABLE;
		if (!(event.packet->flags & ENET_PACKET_FLAG_UNSEQUENCED))
			flags &= FLAG_SEQUENCED;
		Peer *peer = (Peer *)event.peer->data;
		peer->CallCallbackReceive(event.packet->data, event.packet->dataLength,
								  flags);
		enet_packet_destroy(event.packet);
	} break;
	case ENET_EVENT_TYPE_DISCONNECT: {
		Peer *peer = (Peer *)event.peer->data;
		peer->CallCallbackDisconnect(event.data);
		peers.erase(((Peer *)(event.peer->data))->shared_from_this());
	} break;
	case ENET_EVENT_TYPE_NONE:;
	}
}

void Host::DispatchAllEventsFromQueue()
{
	for (int i = 0; i < 10; ++i) {
		popedCommands.clear();
		commandQueue->TryDequeueBulkAny(popedCommands);

		if (popedCommands.empty())
			return;
		for (Command &c : popedCommands) {
			c.Execute();
			++i;
		}
		popedCommands.clear();
	}
}

void Host::SetConnect(void (*callback)(Peer *))
{
	callbackOnConnect = callback;
}

void Host::SetReceive(void (*callback)(Peer *, std::vector<uint8_t> &data,
									   uint32_t flags))
{
	callbackOnReceive = callback;
}

void Host::SetDisconnect(void (*callback)(Peer *, uint32_t disconnectData))
{
	callbackOnDisconnect = callback;
}

void Host::Stop() { flags |= TO_STOP; }

void Host::WaitStop()
{
	while (flags & RUNNING) {
		Stop();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

std::future<std::shared_ptr<Peer>> Host::ConnectPromise(std::string address,
														uint16_t port)
{
	std::shared_ptr<std::promise<std::shared_ptr<Peer>>> promise =
		std::make_shared<std::promise<std::shared_ptr<Peer>>>();
	commands::ExecuteOnPeer onConnected;
	onConnected.customSharedData = promise;
	onConnected.function = [](auto peer, auto data, auto customSharedData) {
		std::shared_ptr<std::promise<std::shared_ptr<Peer>>> promise =
			std::static_pointer_cast<std::promise<std::shared_ptr<Peer>>>(
				customSharedData);
		promise->set_value(peer);
	};
	auto future = promise->get_future();
	Connect(address, port, std::move(onConnected), nullptr);
	return future;
}

void Host::Connect(std::string address, uint16_t port,
				   commands::ExecuteOnPeer &&onConnected,
				   std::shared_ptr<CommandExecutionQueue> queue)
{
	std::thread(
		[](std::string address, uint16_t port, std::shared_ptr<Host> host,
		   commands::ExecuteOnPeer &&onConnected,
		   std::shared_ptr<CommandExecutionQueue> queue) {
			Command command(commands::ExecuteConnect{});
			commands::ExecuteConnect &com = command.executeConnect;
			com.onConnected = std::move(onConnected);
			com.executionQueue = queue;
			enet_address_set_host(&com.address, address.c_str());
			com.address.port = port;
			com.host = host;
			host->EnqueueCommand(std::move(command));
		},
		address, port, shared_from_this(), std::move(onConnected), queue)
		.detach();
}

std::shared_ptr<Peer> Host::_InternalConnect(ENetAddress address)
{
	ENetPeer *peer = enet_host_connect(host, &address, 2, 0);
	enet_host_flush(host);
	std::shared_ptr<Peer> p(new Peer(shared_from_this(), peer));
	peer->data = p.get();
	peers.insert(p);
	return p;
}

void Host::EnqueueCommand(Command &&command)
{
	commandQueue->EnqueueCommand(std::move(command));
}

void Host::SetMessagePassingEnvironment(
	std::shared_ptr<MessagePassingEnvironment> mpe)
{
	this->mpe = mpe;
	if (mpe) {
		this->callbackOnReceive = [](Peer *peer, std::vector<uint8_t> &data,
									 uint32_t flags) {
			peer->GetHost()->GetMessagePassingEnvironment()->OnReceive(
				peer, data, flags);
		};
	}
}

uint32_t Initialize() { return enet_initialize(); }

void Deinitialize() { enet_deinitialize(); }
} // namespace icon6
