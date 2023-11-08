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
	std::lock_guard lock(mutex);
	fprintf(stdout, "%s:%i\t\t [ %2i ] \t ", file, line, id);
	vfprintf(stdout, fmt, va);
	fprintf(stdout, "\n");
	fflush(stdout);
}

Host *Host::_InternalGetSetThreadLocalHost(bool set, Host *host)
{
	static thread_local Host *_host = nullptr;
	if (set) {
		_host = host;
	}
	return _host;
}

Host *Host::GetThreadLocalHost()
{
	return _InternalGetSetThreadLocalHost(false, nullptr);
}

void Host::SetThreadLocalHost(Host *host)
{
	_InternalGetSetThreadLocalHost(true, host);
}

Host::Host(uint16_t port)
{
	SteamNetworkingIPAddr serverLocalAddr;
	serverLocalAddr.Clear();
	serverLocalAddr.m_port = port;
	Init(&serverLocalAddr);
}

Host::Host() { Init(nullptr); }

Host::~Host() { Destroy(); }

void Host::StaticSteamNetConnectionStatusChangedCallback(
	SteamNetConnectionStatusChangedCallback_t *pInfo)
{
	Host *host = Host::GetThreadLocalHost();
	host->SteamNetConnectionStatusChangedCallback(pInfo);
}

void Host::Init(const SteamNetworkingIPAddr *address)
{
	mpe = nullptr;
	commandQueue = new CommandExecutionQueue();
	flags = 0;
	callbackOnConnect = nullptr;
	callbackOnReceive = nullptr;
	callbackOnDisconnect = nullptr;
	userData = 0;
	userPointer = nullptr;

	host = SteamNetworkingSockets();
	if (address) {
		SteamNetworkingConfigValue_t opt;
		opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
				   (void *)Host::StaticSteamNetConnectionStatusChangedCallback);
		listeningSocket = host->CreateListenSocketIP(*address, 1, &opt);
	} else {
		listeningSocket = k_HSteamListenSocket_Invalid;
	}

	pollGroup = host->CreatePollGroup();
}

void Host::Destroy()
{
	if (host) {
		DispatchAllEventsFromQueue();
		WaitStop();
		DispatchAllEventsFromQueue();
		for (auto it : peers) {
			it.second->_InternalDisconnect();
			delete it.second;
		}
		peers.clear();
		peersQueuedSends.clear();
		if (listeningSocket != k_HSteamListenSocket_Invalid) {
			host->CloseListenSocket(listeningSocket);
			listeningSocket = k_HSteamListenSocket_Invalid;
		}
		host->DestroyPollGroup(pollGroup);
		pollGroup = k_HSteamNetPollGroup_Invalid;
		host = nullptr;
		delete commandQueue;
		commandQueue = nullptr;
	}
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
	peersQueuedSends.clear();
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
			host->RunCallbacks();
			int receivedMessages = 0;
			for (receivedMessages = 0; receivedMessages < 512;
				 ++receivedMessages) {
				ISteamNetworkingMessage *msg = nullptr;
				int numMsgs =
					host->ReceiveMessagesOnPollGroup(pollGroup, &msg, 1);
				if (numMsgs == 1) {
					Peer *peer = (Peer *)msg->m_nConnUserData;
					peer->CallCallbackReceive(msg);
				} else {
					break;
				}
			}
			if (mpe != nullptr) {
				mpe->CheckForTimeoutFunctionCalls(6);
			}
			FlushPeersQueuedSends(16);
			uint32_t dispatchedNum = DispatchAllEventsFromQueue(512);
			if (receivedMessages == 0) {
				if (dispatchedNum == 0) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
	case k_ESteamNetworkingConnectionState_ClosedByPeer:
	case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
		if (pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connected) {
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

		host->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
		break;
	}

	case k_ESteamNetworkingConnectionState_Connecting: {
		if (peers.find(pInfo->m_hConn) != peers.end()) { // this is outgoing
														 // connection, thus
														 // icon6::Peer* object
														 // already exists.
			break;
		}
		if (host->AcceptConnection(pInfo->m_hConn) != k_EResultOK) {
			host->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
			DEBUG("Can't accept connection.  (It was already closed?)");
			break;
		}

		if (!host->SetConnectionPollGroup(pInfo->m_hConn, pollGroup)) {
			host->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
			DEBUG("Failed to set poll group?");
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

		break;
	}

	case k_ESteamNetworkingConnectionState_Connected: {
		Peer *peer = (Peer *)host->GetConnectionUserData(pInfo->m_hConn);
		peer->SetReadyToUse();
		if (callbackOnConnect) {
			callbackOnConnect(peer);
		}
		break;
	}

	case k_ESteamNetworkingConnectionState_Dead:
	case k_ESteamNetworkingConnectionState_Linger:
	case k_ESteamNetworkingConnectionState_FinWait:
	case k_ESteamNetworkingConnectionState_FindingRoute:
	case k_ESteamNetworkingConnectionState__Force32Bit:
		break;
	}
}

uint32_t Host::DispatchAllEventsFromQueue(uint32_t maxEvents)
{
	const uint32_t MAX_DEQUEUE_COMMANDS = 128;
	maxEvents = std::min(maxEvents, MAX_DEQUEUE_COMMANDS);
	Command commands[MAX_DEQUEUE_COMMANDS];
	const uint32_t dequeued =
		commandQueue->TryDequeueBulkAny(commands, maxEvents);

	for (uint32_t i = 0; i < dequeued; ++i) {
		commands[i].Execute();
		commands[i].~Command();
	}
	return dequeued;
}

void Host::SetConnect(void (*callback)(Peer *))
{
	callbackOnConnect = callback;
}

void Host::SetReceive(void (*callback)(Peer *, ByteReader &, Flags flags))
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
	std::promise<Peer *> *promise = new std::promise<Peer *>();
	commands::ExecuteOnPeer onConnected;
	onConnected.userPointer = promise;
	onConnected.function = [](auto peer, auto data, auto userPointer) {
		std::promise<Peer *> *promise = (std::promise<Peer *> *)(userPointer);
		promise->set_value(peer);
		delete promise;
	};
	auto future = promise->get_future();
	Connect(address, port, std::move(onConnected), nullptr);
	return future;
}

void Host::Connect(std::string address, uint16_t port)
{
	commands::ExecuteOnPeer com;
	com.function = [](auto a, auto b, auto c) {};
	Connect(address, port, std::move(com), nullptr);
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
			commands::ExecuteConnect &com =
				std::get<commands::ExecuteConnect>(command.cmd);
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
	opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
			   (void *)Host::StaticSteamNetConnectionStatusChangedCallback);

	auto connection = host->ConnectByIPAddress(*address, 1, &opt);

	if (!host->SetConnectionPollGroup(connection, pollGroup)) {
		host->CloseConnection(connection, 0, nullptr, false);
		DEBUG("Failed to set poll group?");
		return nullptr;
	}

	Peer *p = new Peer(this, connection);
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

void Host::FlushPeersQueuedSends(int amount)
{
	if (!peersQueuedSends.empty()) {
		if (peersQueuedSendsIterator == peersQueuedSends.end()) {
			peersQueuedSendsIterator = peersQueuedSends.begin();
		}
		Peer *lastChecked = nullptr;
		std::vector<Peer *> toRemove;
		for (int i = 0;
			 peersQueuedSendsIterator != peersQueuedSends.end() && i < amount;
			 ++peersQueuedSendsIterator, ++i) {
			Peer *peer = *peersQueuedSendsIterator;
			peer->_InternalFlushQueuedSends();
			if (peer->queuedSends.empty()) {
				toRemove.emplace_back(peer);
			}
			lastChecked = peer;
		}
		for (Peer *peer : toRemove) {
			peersQueuedSends.erase(peer);
		}
		peersQueuedSendsIterator = peersQueuedSends.find(lastChecked);
	}
}

static bool _enableSteamNetworkingDebugPrinting = false;

void EnableSteamNetworkingDebug(bool value)
{
	_enableSteamNetworkingDebugPrinting = value;
}

uint32_t Initialize()
{
	SteamDatagramErrMsg errMsg;
	if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
		DEBUG("GameNetworkingSockets_Init failed: %s",
			  ((std::string)errMsg).c_str());
		return -1;
	}

	SteamNetworkingUtils()->SetDebugOutputFunction(
		k_ESteamNetworkingSocketsDebugOutputType_Msg,
		[](ESteamNetworkingSocketsDebugOutputType eType, const char *msg) {
			if (_enableSteamNetworkingDebugPrinting) {
				DEBUG("%s     ::: type(%i)", msg, eType);
			}
		});
	return 0;
}

void Deinitialize() { GameNetworkingSockets_Kill(); }
} // namespace icon6
