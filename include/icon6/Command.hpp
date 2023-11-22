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

#include <vector>
#include <variant>

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
class ExecuteOnPeerNoArgs final
{
public:
	ExecuteOnPeerNoArgs(ExecuteOnPeerNoArgs &&) = default;
	ExecuteOnPeerNoArgs &operator=(ExecuteOnPeerNoArgs &&) = default;

	Peer *peer;
	void (*function)(Peer *peer);

	void Execute();
};

class ExecuteOnPeer final
{
public:
	ExecuteOnPeer() = default;
	ExecuteOnPeer(ExecuteOnPeer &&) = default;
	ExecuteOnPeer &operator=(ExecuteOnPeer &&) = default;

	Peer *peer;
	std::vector<uint8_t> data;
	void *userPointer;
	void (*function)(Peer *peer, std::vector<uint8_t> &data,
					 void *customSharedData);

	void Execute();
};

class ExecuteRPC final
{
public:
	ExecuteRPC &operator=(ExecuteRPC &&) = default;

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

	void Execute();
};

class ExecuteRMI final
{
public:
	ExecuteRMI &operator=(ExecuteRMI &&) = default;

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

	void Execute();
};

class ExecuteReturnRC final
{
public:
	ExecuteReturnRC &operator=(ExecuteReturnRC &&) = default;

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

	void Execute();
};

class ExecuteConnectGNS final
{
public:
	ExecuteConnectGNS(ExecuteConnectGNS &&) = default;
	ExecuteConnectGNS &operator=(ExecuteConnectGNS &&) = default;

	Host *host;
	SteamNetworkingIPAddr address;

	CommandExecutionQueue *executionQueue;
	ExecuteOnPeer onConnected;

	void Execute();
};

class ExecuteSend final
{
public:
	ExecuteSend(ExecuteSend &&) = default;
	ExecuteSend &operator=(ExecuteSend &&) = default;

	std::vector<uint8_t> data;
	Peer *peer;
	Flags flags;

	void Execute();
};

class ExecuteDisconnect final
{
public:
	ExecuteDisconnect(ExecuteDisconnect &&) = default;
	ExecuteDisconnect &operator=(ExecuteDisconnect &&) = default;

	Peer *peer;

	void Execute();
};

class ExecuteFunctionPointer final
{
public:
	ExecuteFunctionPointer(ExecuteFunctionPointer &&) = default;
	ExecuteFunctionPointer &operator=(ExecuteFunctionPointer &&) = default;

	void (*function)();

	void Execute();
};
} // namespace commands

class Command final
{
public:
	Command() : cmd(0) {}
	~Command() = default;
	Command(Command &&other) = default;
	Command(Command &other) = delete;
	Command(const Command &other) = delete;

	Command &operator=(Command &&other) = default;
	Command &operator=(Command &other) = delete;
	Command &operator=(const Command &other) = delete;

	std::variant<int, commands::ExecuteOnPeer, commands::ExecuteOnPeerNoArgs,
				 commands::ExecuteRPC, commands::ExecuteRMI,
				 commands::ExecuteReturnRC, commands::ExecuteConnectGNS,
				 commands::ExecuteSend, commands::ExecuteDisconnect,
				 commands::ExecuteFunctionPointer>
		cmd;

	void Execute();

public:
	Command(commands::ExecuteOnPeer &&executeOnPeer)
		: cmd(std::move(executeOnPeer))
	{
	}
	Command(commands::ExecuteOnPeerNoArgs &&executeOnPeerNoArgs)
		: cmd(std::move(executeOnPeerNoArgs))
	{
	}
	Command(commands::ExecuteRPC &&executeRPC) : cmd(std::move(executeRPC)) {}
	Command(commands::ExecuteRMI &&executeRMI) : cmd(std::move(executeRMI)) {}
	Command(commands::ExecuteReturnRC &&executeReturnRC)
		: cmd(std::move(executeReturnRC))
	{
	}
	Command(commands::ExecuteConnectGNS &&executeConnect)
		: cmd(std::move(executeConnect))
	{
	}
	Command(commands::ExecuteSend &&executeSend) : cmd(std::move(executeSend))
	{
	}
	Command(commands::ExecuteDisconnect &&executeDisconnect)
		: cmd(std::move(executeDisconnect))
	{
	}
	Command(commands::ExecuteFunctionPointer &&executeFunctionPointer)
		: cmd(std::move(executeFunctionPointer))
	{
	}
};
} // namespace icon6

#endif
