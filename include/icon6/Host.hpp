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
#include <future>
#include <atomic>
#include <unordered_map>
#include <vector>

#include "Flags.hpp"
#include "CommandExecutionQueue.hpp"

#define DEBUG(...) icon6::Debug(__FILE__, __LINE__, __VA_ARGS__)

namespace icon6
{
void Debug(const char *file, int line, const char *fmt, ...);

class Peer;
class MessagePassingEnvironment;

class Host
{
public:
	void SetMessagePassingEnvironment(MessagePassingEnvironment *mpe);
	inline MessagePassingEnvironment *GetMessagePassingEnvironment()
	{
		return mpe;
	}

	virtual ~Host();

	// thread unsafe
	virtual void Destroy();

	void RunAsync();
	// thread unsafe
	void RunSync();
	// thread unsafe
	virtual void RunSingleLoop(uint32_t maxWaitTimeMilliseconds = 4) = 0;

	// thread unsafe
	virtual void DisconnectAllGracefully() = 0;

	void SetConnect(void (*callback)(Peer *));
	void SetReceive(void (*callback)(Peer *, ByteReader &, Flags flags));
	void SetDisconnect(void (*callback)(Peer *));

	void Stop();
	void WaitStop();

	// Thread unsafe.
	virtual void ForEachPeer(void(func)(icon6::Peer *)) = 0;
	// Thread unsafe.
	virtual void ForEachPeer(std::function<void(icon6::Peer *)> func) = 0;

public:
	std::future<Peer *> ConnectPromise(std::string address, uint16_t port);
	void Connect(std::string address, uint16_t port);
	virtual void Connect(std::string address, uint16_t port,
						 commands::ExecuteOnPeer &&onConnected,
						 CommandExecutionQueue *queue = nullptr) = 0;

	static Host *GetThreadLocalHost();
	static void SetThreadLocalHost(Host *host);

	void EnqueueCommand(Command &&command);

public:
	uint64_t userData;
	void *userPointer;

	friend class Peer;

protected:
	uint32_t DispatchAllEventsFromQueue(uint32_t maxEventsDispatched = 100);
	void DispatchPopedEventsFromQueue();

	static Host *_InternalGetSetThreadLocalHost(bool set, Host *host);

protected:
	std::unordered_set<Peer *> peersQueuedSends;
	decltype(peersQueuedSends.begin()) peersQueuedSendsIterator;
	void FlushPeersQueuedSends(int amount);

	// create host on given port
	Host(uint16_t port);
	// create client host with automatic port
	Host();

protected:
	MessagePassingEnvironment *mpe;

	enum HostFlags : uint32_t {
		RUNNING = 1 << 0,
		TO_STOP = 1 << 1,
	};

	std::atomic<uint32_t> flags;

	CommandExecutionQueue *commandQueue;

	void (*callbackOnConnect)(Peer *);
	void (*callbackOnReceive)(Peer *, ByteReader &, Flags);
	void (*callbackOnDisconnect)(Peer *);
};

uint32_t Initialize();
void Deinitialize();
} // namespace icon6

#endif
