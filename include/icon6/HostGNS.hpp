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

#ifndef ICON6_HOST_GAME_NETWORKING_SOCKETS_HPP
#define ICON6_HOST_GAME_NETWORKING_SOCKETS_HPP

#include <string>
#include <future>
#include <atomic>
#include <unordered_map>
#include <vector>

#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

#include "Flags.hpp"
#include "CommandExecutionQueue.hpp"

#include "Host.hpp"

namespace icon6
{
void Debug(const char *file, int line, const char *fmt, ...);

class Peer;
class MessagePassingEnvironment;

namespace gns
{
class Peer;

class Host final : public icon6::Host
{
public:
	// create host on given port
	Host(uint16_t port);
	// create client host with automatic port
	Host();
	virtual ~Host() override;

	// thread unsafe
	virtual void Destroy() override;

	virtual void RunSingleLoop(uint32_t maxWaitTimeMilliseconds = 4) override;

	// thread unsafe
	virtual void DisconnectAllGracefully() override;

	// Thread unsafe.
	virtual void ForEachPeer(void(func)(icon6::Peer *)) override;
	// Thread unsafe.
	virtual void ForEachPeer(std::function<void(icon6::Peer *)> func) override;

public:
	inline void Connect(std::string address, uint16_t port)
	{
		icon6::Host::Connect(address, port);
	}
	virtual void Connect(std::string address, uint16_t port,
						 commands::ExecuteOnPeer &&onConnected,
						 CommandExecutionQueue *queue = nullptr) override;

	// thread unsafe.
	gns::Peer *_InternalConnect(const SteamNetworkingIPAddr *address);

public:
	uint64_t userData;
	void *userPointer;

	friend class gns::Peer;

private:
	static void StaticSteamNetConnectionStatusChangedCallback(
		SteamNetConnectionStatusChangedCallback_t *pInfo);
	void Init(const SteamNetworkingIPAddr *address);
	void SteamNetConnectionStatusChangedCallback(
		SteamNetConnectionStatusChangedCallback_t *pInfo);

private:
	std::unordered_set<Peer *> peersQueuedSends;
	decltype(peersQueuedSends.begin()) peersQueuedSendsIterator;

private:
	MessagePassingEnvironment *mpe;

	enum HostFlags : uint32_t {
		RUNNING = 1 << 0,
		TO_STOP = 1 << 1,
	};

	ISteamNetworkingSockets *host;
	HSteamListenSocket listeningSocket;
	HSteamNetPollGroup pollGroup;

	std::unordered_map<HSteamNetConnection, gns::Peer *> peers;
};

void EnableSteamNetworkingDebug(bool value);

} // namespace gns
} // namespace icon6

#endif
