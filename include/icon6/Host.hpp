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

#include <string>
#include <memory>
#include <future>
#include <atomic>
#include <unordered_map>
#include <vector>

#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

#include "Flags.hpp"
#include "Peer.hpp"
#include "Command.hpp"
#include "CommandExecutionQueue.hpp"

#define DEBUG(...) icon6::Debug(__FILE__, __LINE__, __VA_ARGS__)

namespace icon6
{
void Debug(const char *file, int line, const char *fmt, ...);

class Peer;
class ConnectionEncryptionState;
class MessagePassingEnvironment;

enum class PeerAcceptancePolicy {
	ACCEPT_ALL,
	ACCEPT_DIRECTLY_TRUSTED,
	ACCEPT_INDIRECTLY_TRUSTED_BY_CA,
};

class Host final
{
public:
	void SetMessagePassingEnvironment(MessagePassingEnvironment *mpe);
	inline MessagePassingEnvironment *GetMessagePassingEnvironment()
	{
		return mpe;
	}

	// create host on given port
	Host(uint16_t port);
	// create client host with automatic port
	Host();
	~Host();

	// thread unsafe
	void Destroy();

	void RunAsync();
	// thread unsafe
	void RunSync();
	// thread unsafe
	void RunSingleLoop(uint32_t maxWaitTimeMilliseconds = 4);

	// thread unsafe
	void DisconnectAllGracefully();

	void SetConnect(void (*callback)(Peer *));
	void SetReceive(void (*callback)(Peer *, ByteReader &, Flags flags));
	void SetDisconnect(void (*callback)(Peer *));

	void Stop();
	void WaitStop();

	// Thread unsafe.
	template <typename TFunc> void ForEachPeer(TFunc &&func)
	{
		for (auto it : peers) {
			func(it.second);
		}
	}

public:
	std::future<Peer *> ConnectPromise(std::string address, uint16_t port);
	void Connect(std::string address, uint16_t port);
	void Connect(std::string address, uint16_t port,
				 commands::ExecuteOnPeer &&onConnected,
				 CommandExecutionQueue *queue = nullptr);

	// thread unsafe.
	Peer *_InternalConnect(const SteamNetworkingIPAddr *address);

	static Host *GetThreadLocalHost();
	static void SetThreadLocalHost(Host *host);

public:
	uint64_t userData;
	void *userPointer;
	std::shared_ptr<void> userSharedPointer;

	friend class Peer;
	friend class ConnectionEncryptionState;

private:
	static void StaticSteamNetConnectionStatusChangedCallback(
		SteamNetConnectionStatusChangedCallback_t *pInfo);
	void Init(const SteamNetworkingIPAddr *address);
	void SteamNetConnectionStatusChangedCallback(
		SteamNetConnectionStatusChangedCallback_t *pInfo);
	uint32_t DispatchAllEventsFromQueue(uint32_t maxEventsDispatched = 100);
	void DispatchPopedEventsFromQueue();
	void EnqueueCommand(Command &&command);

	static Host *_InternalGetSetThreadLocalHost(bool set, Host *host);

private:
	std::unordered_set<Peer *> peersQueuedSends;
	decltype(peersQueuedSends.begin()) peersQueuedSendsIterator;
	void FlushPeersQueuedSends(int amount);

private:
	MessagePassingEnvironment *mpe;

	enum HostFlags : uint32_t {
		RUNNING = 1 << 0,
		TO_STOP = 1 << 1,
	};

	ISteamNetworkingSockets *host;
	HSteamListenSocket listeningSocket;
	HSteamNetPollGroup pollGroup;
	std::atomic<uint32_t> flags;

	std::unordered_map<HSteamNetConnection, Peer *> peers;

	CommandExecutionQueue *commandQueue;

	void (*callbackOnConnect)(Peer *);
	void (*callbackOnReceive)(Peer *, ByteReader &, Flags);
	void (*callbackOnDisconnect)(Peer *);
};

void EnableSteamNetworkingDebug(bool value);
uint32_t Initialize();
void Deinitialize();
} // namespace icon6

#endif
