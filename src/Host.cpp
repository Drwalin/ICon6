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

#include <unistd.h>

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

Host* Host::_InternalGetSetThreadLocalHost(bool set, Host *host)
{
	static thread_local Host* _host = nullptr;
	if (set) {
		_host = host;
	}
	return _host;
}

Host* Host::GetThreadLocalHost()
{
	return _InternalGetSetThreadLocalHost(false, nullptr);
}

void Host::SetThreadLocalHost(Host *host)
{
	_InternalGetSetThreadLocalHost(true, host);
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

void Host::StaticSteamNetConnectionStatusChangedCallback(
			SteamNetConnectionStatusChangedCallback_t *pInfo) {
	Host *host = Host::GetThreadLocalHost();
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
	
	host = SteamNetworkingSockets();
	if (address) {
		SteamNetworkingConfigValue_t opt;
		opt.SetPtr( k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
				(void*)Host::StaticSteamNetConnectionStatusChangedCallback);
		listeningSocket = host->CreateListenSocketIP(*address, 1, &opt);
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
	Host::SetThreadLocalHost(this);
	if (flags & TO_STOP) {
		DisconnectAllGracefully();
		flags &= ~RUNNING;
	} else {
		flags |= RUNNING;
		while (!(flags & TO_STOP)) {
			int receivedMessages = 0;
			for (receivedMessages=0; receivedMessages<10; ++receivedMessages) {
				ISteamNetworkingMessage *msg = nullptr;
				int numMsgs = host->ReceiveMessagesOnPollGroup( pollGroup, &msg, 1 );
				if (numMsgs == 1) {
					Peer *peer = (Peer *)msg->m_nConnUserData;
					peer->CallCallbackReceive(msg);
				} else {
					break;
				}
			}
			host->RunCallbacks();
			if (mpe != nullptr) {
				mpe->CheckForTimeoutFunctionCalls(6);
			}
			int dispatchedNum = DispatchAllEventsFromQueue();
			if (receivedMessages == 0) {
				if (dispatchedNum == 0) {
					std::this_thread::sleep_for(std::chrono::microseconds(1000));
				} else if (dispatchedNum < 3) {
					std::this_thread::sleep_for(std::chrono::microseconds(100));
				} else if (dispatchedNum < 9) {
					std::this_thread::yield();
				}
			}
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
			if (peers.find(pInfo->m_hConn) != peers.end()) {
				break;
			}
			if (host->AcceptConnection( pInfo->m_hConn ) != k_EResultOK) {
				// This could fail.  If the remote host tried to connect, but then
				// disconnected, the connection may already be half closed.  Just
				// destroy whatever we have on our side.
				host->CloseConnection( pInfo->m_hConn, 0, nullptr, false );
				DEBUG( "Can't accept connection.  (It was already closed?)" );
				break;
			}
			
			// Assign the poll group
			if ( !host->SetConnectionPollGroup( pInfo->m_hConn, pollGroup ) )
			{
				host->CloseConnection( pInfo->m_hConn, 0, nullptr, false );
				DEBUG( "Failed to set poll group?" );
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

int Host::DispatchAllEventsFromQueue()
{
	int i = 0;
	for (i = 0; i < 10; ++i) {
		popedCommands.clear();
		commandQueue->TryDequeueBulkAny(popedCommands);

		if (popedCommands.empty())
			break;
		for (Command &c : popedCommands) {
			c.Execute();
			++i;
		}
		popedCommands.clear();
	}
	return i;
}

void Host::SetConnect(void (*callback)(Peer *))
{
	callbackOnConnect = callback;
}

void Host::SetReceive(void (*callback)(Peer *, ByteReader &,
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
			com.address.Clear();
			com.address.ParseString(address.c_str());
			com.address.m_port = port;
			com.host = host;
			host->EnqueueCommand(std::move(command));
		},
		address, port, this, std::move(onConnected), queue)
		.detach();
}

Peer *Host::_InternalConnect(const SteamNetworkingIPAddr *address)
{
	SteamNetworkingConfigValue_t opt;
	opt.SetPtr( k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
			(void*)Host::StaticSteamNetConnectionStatusChangedCallback);
	
	auto connection = host->ConnectByIPAddress(*address, 1, &opt);
	
	if (!host->SetConnectionPollGroup(connection, pollGroup)) {
		host->CloseConnection(connection, 0, nullptr, false );
		DEBUG( "Failed to set poll group?" );
		return nullptr;
	}
	
	Peer *p(new Peer(this, connection));
	peers[connection] = p;
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
		this->callbackOnReceive = [](Peer *peer, ByteReader &reader,
									 Flags flags) {
			peer->GetHost()->GetMessagePassingEnvironment()->OnReceive(
				peer, reader, flags);
		};
	}
}

uint32_t Initialize() {
#ifdef STEAMNETWORKINGSOCKETS_OPENSOURCE
	SteamDatagramErrMsg errMsg;
	if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
		throw (std::string("GameNetworkingSockets_Init failed: ") + errMsg);
		return -1;
	}
#else
	SteamDatagram_SetAppID(570); // Just set something, doesn't matter what
	SteamDatagram_SetUniverse(false, k_EUniverseDev);

	SteamDatagramErrMsg errMsg;
	if (!SteamDatagramClient_Init(errMsg))
		FatalError("SteamDatagramClient_Init failed.  %s", errMsg);

	// Disable authentication when running with Steam, for this
	// example, since we're not a real app.
	//
	// Authentication is disabled automatically in the open-source
	// version since we don't have a trusted third party to issue
	// certs.
	SteamNetworkingUtils()->SetGlobalConfigValueInt32(
		k_ESteamNetworkingConfig_IP_AllowWithoutAuth, 1);
#endif

	SteamNetworkingUtils()->SetDebugOutputFunction(
		k_ESteamNetworkingSocketsDebugOutputType_Msg, [](
		ESteamNetworkingSocketsDebugOutputType eType, const char *msg){
			DEBUG("%s     ::: pid=%i, type(%i)", msg, getpid(), eType);
		});
	return 0;
}

void Deinitialize() {
	GameNetworkingSockets_Kill();
}
} // namespace icon6
