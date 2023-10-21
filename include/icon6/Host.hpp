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
#include <unordered_set>
#include <vector>

#include <enet/enet.h>

#include "Flags.hpp"
#include "Peer.hpp"
#include "Command.hpp"
#include "CommandExecutionQueue.hpp"

#include "Cert.hpp"

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
	static Host *Make(uint16_t port, uint32_t maximumHostsNumber);

	void SetMessagePassingEnvironment(MessagePassingEnvironment *mpe);
	inline MessagePassingEnvironment *GetMessagePassingEnvironment()
	{
		return mpe;
	}

	// create host on 127.0.0.1:port
	Host(uint16_t port, uint32_t maximumHostsNumber);

	// create client host on any port
	Host(uint32_t maximumHostsNumber);
	~Host();

	void SetCertificatePolicy(PeerAcceptancePolicy peerAcceptancePolicy);
	void AddTrustedRootCA(crypto::Cert *rootCA);
	void SetSelfCertificate(crypto::Cert *root);
	void InitRandomSelfsignedCertificate();
	void SetPolicyMaximumDepthOfCertificateAcceptable(uint32_t maxDepth);

	void Destroy();

	void RunAsync();
	void RunSync();
	void RunSingleLoop(uint32_t maxWaitTimeMilliseconds = 4);

	void DisconnectAllGracefully();

	void SetConnect(void (*callback)(Peer *));
	void SetReceive(void (*callback)(Peer *, std::vector<uint8_t> &data,
									 Flags flags));
	void SetDisconnect(void (*callback)(Peer *, uint32_t disconnectData));

	void Stop();
	void WaitStop();

public:
	// thread safe function to connect to a remote host
	std::future<Peer *> ConnectPromise(std::string address, uint16_t port);
	// thread safe function to connect to a remote host
	void Connect(std::string address, uint16_t port,
				 commands::ExecuteOnPeer &&onConnected,
				 CommandExecutionQueue *queue = nullptr);

	Peer *_InternalConnect(ENetAddress address);

public:
	void *userData;
	std::shared_ptr<void> userSharedPointer;

	friend class Peer;
	friend class ConnectionEncryptionState;

private:
	void Init(ENetAddress *address, uint32_t maximumHostsNumber);
	void DispatchEvent(ENetEvent &event);
	void DispatchAllEventsFromQueue();
	void DispatchPopedEventsFromQueue();
	void EnqueueCommand(Command &&command);

private:
	// 		crypto::Cert *cert;
	crypto::CertKey *certKey;
	// 		std::vector<crypto::Cert *> trustedRootCertificates;
	PeerAcceptancePolicy peerAcceptancePolicy =
		PeerAcceptancePolicy::ACCEPT_ALL;
	// 		uint32_t maxCertificateDepthAcceptable = 16;

	MessagePassingEnvironment *mpe;

	enum HostFlags : uint32_t {
		RUNNING = 1 << 0,
		TO_STOP = 1 << 1,
	};

	ENetHost *host;
	std::atomic<uint32_t> flags;

	std::unordered_set<Peer *> peers;

	CommandExecutionQueue *commandQueue;
	std::vector<Command> popedCommands;

	void (*callbackOnConnect)(Peer *);
	void (*callbackOnReceive)(Peer *, std::vector<uint8_t> &data, Flags flags);
	void (*callbackOnDisconnect)(Peer *, uint32_t disconnectData);
};

uint32_t Initialize();
void Deinitialize();
} // namespace icon6

#endif
