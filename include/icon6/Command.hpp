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

#ifndef ICON6_COMMAND_HPP
#define ICON6_COMMAND_HPP

#include <cinttypes>

#include <string>
#include <vector>

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

#include "Flags.hpp"
#include "ByteReader.hpp"

namespace icon6
{

class Host;
class Peer;
class MessageConverter;
namespace rmi
{
class MethodInvocationConverter;
}
class Command;
class CommandExecutionQueue;

namespace commands
{

class BaseCommandExecute
{
public:
	virtual ~BaseCommandExecute() = default;

	virtual void Execute() = 0;
};

class ExecuteOnPeerNoArgs final : public BaseCommandExecute
{
public:
	Peer *peer;
	void (*function)(Peer *peer);

	virtual void Execute() override;
};

class ExecuteOnPeer final : public BaseCommandExecute
{
public:
	Peer *peer;
	std::vector<uint8_t> data;
	void *userPointer;
	void (*function)(Peer *peer, std::vector<uint8_t> &data,
					 void *customSharedData);

	virtual void Execute() override;
};

class ExecuteRPC final : public BaseCommandExecute
{
public:
	ExecuteRPC(ByteReader &&reader) : reader(std::move(reader)) {}
	ExecuteRPC(ExecuteRPC &&o)
		: peer(std::move(o.peer)), reader(std::move(o.reader)),
		  messageConverter(std::move(o.messageConverter)), flags(o.flags)
	{
	}

	Peer *peer;
	ByteReader reader;
	MessageConverter *messageConverter;
	Flags flags;

	virtual void Execute() override;
};

class ExecuteRMI final : public BaseCommandExecute
{
public:
	ExecuteRMI(ByteReader &&reader) : reader(std::move(reader)) {}
	ExecuteRMI(ExecuteRMI &&o)
		: peer(std::move(o.peer)), reader(std::move(o.reader)),
		  methodInvoker(std::move(o.methodInvoker)), flags(o.flags)
	{
	}

	Peer *peer;
	ByteReader reader;
	void *objectPtr;
	rmi::MethodInvocationConverter *methodInvoker;
	Flags flags;

	virtual void Execute() override;
};

class ExecuteReturnRC final : public BaseCommandExecute
{
public:
	ExecuteReturnRC(ByteReader &&reader) : reader(std::move(reader)) {}
	ExecuteReturnRC(ExecuteReturnRC &&o)
		: peer(std::move(o.peer)), reader(std::move(o.reader)),
		  function(std::move(o.function)), flags(o.flags)
	{
	}

	Peer *peer;
	void *funcPtr;
	ByteReader reader;
	void (*function)(Peer *, Flags, ByteReader &, void *);
	Flags flags;

	virtual void Execute() override;
};

class ExecuteConnect final : public BaseCommandExecute
{
public:
	Host *host;
	SteamNetworkingIPAddr address;

	CommandExecutionQueue *executionQueue;
	ExecuteOnPeer onConnected;

	virtual void Execute() override;
};

class ExecuteSend final : public BaseCommandExecute
{
public:
	std::vector<uint8_t> data;
	Peer *peer;
	Flags flags;

	virtual void Execute() override;
};

class ExecuteDisconnect final : public BaseCommandExecute
{
public:
	Peer *peer;

	virtual void Execute() override;
};

class ExecuteFunctionPointer final : public BaseCommandExecute
{
public:
	void (*function)();

	virtual void Execute() override;
};
} // namespace commands

class Command final
{
public:
	Command();
	~Command();
	Command(Command &&other);
	Command(Command &other) = delete;
	Command(const Command &other) = delete;

	Command &operator=(Command &&other);
	Command &operator=(Command &other) = delete;
	Command &operator=(const Command &other) = delete;

	template <typename T> T *Ptr() { return (T *)&executeOnPeer; }
	template <typename T> T &Ref() { return *(T *)&executeOnPeer; }

	union {
		commands::ExecuteOnPeer executeOnPeer;
		commands::ExecuteOnPeerNoArgs executeOnPeerNoArgs;
		commands::ExecuteRPC executeRPC;
		commands::ExecuteRMI executeRMI;
		commands::ExecuteReturnRC executeReturnRC;
		commands::ExecuteConnect executeConnect;
		commands::ExecuteSend executeSend;
		commands::ExecuteDisconnect executeDisconnect;
		commands::ExecuteFunctionPointer executeFunctionPointer;
	};

	bool hasValue;

	void Execute();

public:
	Command(commands::ExecuteOnPeer &&executeOnPeer)
		: executeOnPeer(), hasValue(true)
	{
		this->executeOnPeer = std::move(executeOnPeer);
	}
	Command(commands::ExecuteOnPeerNoArgs &&executeOnPeerNoArgs)
		: executeOnPeerNoArgs(), hasValue(true)
	{
		this->executeOnPeerNoArgs = std::move(executeOnPeerNoArgs);
	}
	Command(commands::ExecuteRPC &&executeRPC)
		: executeRPC(std::move(executeRPC)), hasValue(true)
	{
	}
	Command(commands::ExecuteRMI &&executeRMI)
		: executeRMI(std::move(executeRMI)), hasValue(true)
	{
	}
	Command(commands::ExecuteReturnRC &&executeReturnRC)
		: executeReturnRC(std::move(executeReturnRC)), hasValue(true)
	{
	}
	Command(commands::ExecuteConnect &&executeConnect)
		: executeConnect(), hasValue(true)
	{
		this->executeConnect = std::move(executeConnect);
	}
	Command(commands::ExecuteSend &&executeSend) : executeSend(), hasValue(true)
	{
		this->executeSend = std::move(executeSend);
	}
	Command(commands::ExecuteDisconnect &&executeDisconnect)
		: executeDisconnect(), hasValue(true)
	{
		this->executeDisconnect = std::move(executeDisconnect);
	}
	Command(commands::ExecuteFunctionPointer &&executeFunctionPointer)
		: executeFunctionPointer(), hasValue(true)
	{
		this->executeFunctionPointer = std::move(executeFunctionPointer);
	}
};
} // namespace icon6

#endif
