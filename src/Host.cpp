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

#include "../include/icon6/Peer.hpp"
#include "../include/icon6/MessagePassingEnvironment.hpp"
#include "../include/icon6/HostGNS.hpp"

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

Host *Host::MakeGameNetworkingSocketsHost(uint16_t port)
{
	return new gns::Host(port);
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

Host::Host() {}

Host::~Host() { Destroy(); }

void Host::Destroy()
{
	if (commandQueue) {
		DispatchAllEventsFromQueue();
		WaitStop();
		DispatchAllEventsFromQueue();
		peersQueuedSends.clear();
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

uint32_t Initialize() { return gns::Initialize(); }

void Deinitialize() { gns::Deinitialize(); }
} // namespace icon6
