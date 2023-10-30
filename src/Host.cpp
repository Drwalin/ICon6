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

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

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


namespace steam_listen_socket_to_Host_transator
{
	std::mutex mutex;
	std::unordered_map<HSteamListenSocket, Host*> hosts;
	
	void SetHost(HSteamListenSocket listenSocket, Host* host)
	{
		std::lock_guard lock(mutex);
		hosts[listenSocket] = host;
	}
	
	Host* GetHost(SteamNetConnectionStatusChangedCallback_t *pInfo)
	{
		if (pInfo->m_info.m_hListenSocket == k_HSteamListenSocket_Invalid) {
			return ((Peer*)pInfo->m_info.m_nUserData)->GetHost();
		} else {
			std::lock_guard lock(mutex);
			return hosts[pInfo->m_info.m_hListenSocket];
		}
	}
}

Host::Host(uint16_t port, uint32_t maximumHostsNumber)
{
	SteamNetworkingIPAddr serverLocalAddr;
	serverLocalAddr.Clear();
	serverLocalAddr.m_port = port;
	Init(&serverLocalAddr);
}

Host::Host(uint32_t maximumHosts) {
	Init(nullptr);
}

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

void Host::AddTrustedRootCA(crypto::Cert *rootCA)
{
	throw "Host::SetSelfCertificate not implemented yet. Please use only "
		  "Host::SetCertificatePolicy(PeerAcceptancePolicy::ACCEPT_ALL)";
}

void Host::SetSelfCertificate(crypto::Cert *root)
{
	throw "Host::SetSelfCertificate not implemented yet. Please use only "
		  "Host::InitRandomSelfsignedCertificate()";
}

void Host::InitRandomSelfsignedCertificate()
{
	certKey = crypto::CertKey::GenerateKey();
	// TODO: generate self signed certificate
}

void Host::StaticSteamNetConnectionStatusChangedCallback(
			SteamNetConnectionStatusChangedCallback_t *pInfo) {
	Host *host = steam_listen_socket_to_Host_transator::GetHost(pInfo);
	host->SteamNetConnectionStatusChangedCallback(pInfo);
}


void Host::Init(const SteamNetworkingIPAddr *address)
{
	commandQueue = new CommandExecutionQueue();
	flags = 0;
	callbackOnConnect = nullptr;
	callbackOnReceive = nullptr;
	callbackOnDisconnect = nullptr;
	userData = nullptr;
	SetCertificatePolicy(PeerAcceptancePolicy::ACCEPT_ALL);
	InitRandomSelfsignedCertificate();
	
	host = SteamNetworkingSockets();
	if (address) {
		SteamNetworkingConfigValue_t opt;
		opt.SetPtr( k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
				(void*)Host::StaticSteamNetConnectionStatusChangedCallback);
		listeningSocket = host->CreateListenSocketIP(*address, 1, &opt);
		steam_listen_socket_to_Host_transator::SetHost(listeningSocket, this);
	} else {
		listeningSocket = k_HSteamListenSocket_Invalid;
	}
	
	pollGroup = host->CreatePollGroup();
}

void Host::Destroy()
{
	DispatchAllEventsFromQueue();
	WaitStop();
	DispatchAllEventsFromQueue();
	for (auto it : peers) {
		it.second->_InternalDisconnect();
		delete it.second;
	}
	peers.clear();
	host->CloseListenSocket(listeningSocket);
	listeningSocket = k_HSteamListenSocket_Invalid;
	host->DestroyPollGroup( pollGroup );
	pollGroup = k_HSteamNetPollGroup_Invalid;
	host = nullptr;
	delete commandQueue;
	commandQueue = nullptr;
}

void Host::RunAsync()
{
	flags = 0;
	std::thread([](Host *host) { host->RunSync(); }, this).detach();
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
		p.second->_InternalDisconnect();
	}
}

void Host::RunSingleLoop(uint32_t maxWaitTimeMilliseconds)
{
	if (flags & TO_STOP) {
		DisconnectAllGracefully();
		flags &= ~RUNNING;
	} else {
		flags |= RUNNING;
		while (!(flags & TO_STOP)) {
			ISteamNetworkingMessage *msg = nullptr;
			int numMsgs = host->ReceiveMessagesOnPollGroup( pollGroup, &msg, 1 );
			if (numMsgs == 1) {
				Peer *peer = (Peer *)msg->m_nConnUserData;
				peer->CallCallbackReceive(msg);
				msg->Release();
			}
			
			if (mpe != nullptr) {
				mpe->CheckForTimeoutFunctionCalls(6);
			}
			DispatchAllEventsFromQueue();
		}
	}
}

void Host::SteamNetConnectionStatusChangedCallback(
		SteamNetConnectionStatusChangedCallback_t *pInfo)
{
	switch (pInfo->m_info.m_eState) {
		case k_ESteamNetworkingConnectionState_None:
			// NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
			break;

		case k_ESteamNetworkingConnectionState_ClosedByPeer:
		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
		{
			// Ignore if they were not previously connected.  (If they disconnected
			// before we accepted the connection.)
			if ( pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connected )
			{
				auto userData = host->GetConnectionUserData(pInfo->m_hConn);
				Peer *peer = nullptr;
				if (userData == -1) {
					peer = new Peer(this, pInfo->m_hConn);
					peers[pInfo->m_hConn] = peer;
				} else {
					peer = (Peer *)userData;
				}
				
				if (peer) {
					peer->CallCallbackDisconnect();
					// TODO: delay Peer object destruction
 					// delete peer;
					peers.erase(pInfo->m_hConn);
				}
			}

			// Clean up the connection.  This is important!
			// The connection is "closed" in the network sense, but
			// it has not been destroyed.  We must close it on our end, too
			// to finish up.  The reason information do not matter in this case,
			// and we cannot linger because it's already closed on the other end,
			// so we just pass 0's.
			host->CloseConnection( pInfo->m_hConn, 0, nullptr, false );
			break;
		}

		case k_ESteamNetworkingConnectionState_Connecting:
		{
			// A client is attempting to connect
			// Try to accept the connection.
			if (host->AcceptConnection( pInfo->m_hConn ) != k_EResultOK) {
				// This could fail.  If the remote host tried to connect, but then
				// disconnected, the connection may already be half closed.  Just
				// destroy whatever we have on our side.
				host->CloseConnection( pInfo->m_hConn, 0, nullptr, false );
				break;
			}
			
			auto userData = host->GetConnectionUserData(pInfo->m_hConn);
			Peer *peer = nullptr;
			if (userData == -1) {
				peer = new Peer(this, pInfo->m_hConn);
				peers[pInfo->m_hConn] = peer;
			} else {
				peer = (Peer *)userData;
			}
			
			if (callbackOnConnect) {
				callbackOnConnect(peer);
			}
			break;
		}

		case k_ESteamNetworkingConnectionState_Connected:
			// We will get a callback immediately after accepting the connection.
			// Since we are the server, we can ignore this, it's not news to us.
			break;

		default:
			// Silences -Wswitch
			break;
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
									   Flags flags))
{
	callbackOnReceive = callback;
}

void Host::SetDisconnect(void (*callback)(Peer *))
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

std::future<Peer *> Host::ConnectPromise(std::string address, uint16_t port)
{
	std::shared_ptr<std::promise<Peer *>> promise =
		std::make_shared<std::promise<Peer *>>();
	commands::ExecuteOnPeer onConnected;
	onConnected.customSharedData = promise;
	onConnected.function = [](auto peer, auto data, auto customSharedData) {
		std::shared_ptr<std::promise<Peer *>> promise =
			std::static_pointer_cast<std::promise<Peer *>>(customSharedData);
		promise->set_value(peer);
	};
	auto future = promise->get_future();
	Connect(address, port, std::move(onConnected), nullptr);
	return future;
}

void Host::Connect(std::string address, uint16_t port,
				   commands::ExecuteOnPeer &&onConnected,
				   CommandExecutionQueue *queue)
{
	std::thread(
		[](std::string address, uint16_t port, Host *host,
		   commands::ExecuteOnPeer &&onConnected,
		   CommandExecutionQueue *queue) {
			Command command(commands::ExecuteConnect{});
			commands::ExecuteConnect &com = command.executeConnect;
			com.onConnected = std::move(onConnected);
			com.executionQueue = queue;
			enet_address_set_host(&com.address, address.c_str());
			com.address.port = port;
			com.host = host;
			host->EnqueueCommand(std::move(command));
		},
		address, port, this, std::move(onConnected), queue)
		.detach();
}

Peer *Host::_InternalConnect(ENetAddress address)
{
	ENetPeer *peer = enet_host_connect(host, &address, 2, 0);
	enet_host_flush(host);
	Peer *p(new Peer(this, peer));
	peer->data = p;
	peers.insert(p);
	return p;
}

void Host::EnqueueCommand(Command &&command)
{
	commandQueue->EnqueueCommand(std::move(command));
}

void Host::SetMessagePassingEnvironment(MessagePassingEnvironment *mpe)
{
	this->mpe = mpe;
	if (mpe) {
		this->callbackOnReceive = [](Peer *peer, std::vector<uint8_t> &data,
									 Flags flags) {
			ByteReader reader(std::move(data), 0);
			peer->GetHost()->GetMessagePassingEnvironment()->OnReceive(
				peer, reader, flags);
		};
	}
}

uint32_t Initialize() { return enet_initialize(); }

void Deinitialize() { enet_deinitialize(); }
} // namespace icon6
