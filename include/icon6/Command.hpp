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
#include <memory>
#include <vector>
#include <variant>
#include <future>
#include <functional>

#include <enet/enet.h>

#include "Flags.hpp"

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

class ExecuteOnPeer final : public BaseCommandExecute
{
public:
	std::shared_ptr<Peer> peer;
	std::vector<uint8_t> data;
	std::shared_ptr<void> customSharedData;
	void (*function)(std::shared_ptr<Peer> peer, std::vector<uint8_t> &data,
					 std::shared_ptr<void> customSharedData);

	virtual void Execute() override;
};

class ExecuteFunctionObjectOnPeer final : public BaseCommandExecute
{
public:
	std::shared_ptr<Peer> peer;
	std::vector<uint8_t> data;
	std::shared_ptr<void> customSharedData;
	std::function<void(std::shared_ptr<Peer>, std::vector<uint8_t> &,
					   std::shared_ptr<void> customSharedData)>
		function;

	virtual void Execute() override;
};

class ExecuteFunctionObjectNoArgsOnPeer final : public BaseCommandExecute
{
public:
	std::shared_ptr<Peer> peer;
	std::function<void(std::shared_ptr<Peer>)> function;

	virtual void Execute() override;
};

class ExecuteRPC final : public BaseCommandExecute
{
public:
	std::shared_ptr<Peer> peer;
	std::vector<uint8_t> binaryData;
	std::shared_ptr<MessageConverter> messageConverter;
	uint32_t readOffset;
	Flags flags;

	virtual void Execute() override;
};

class ExecuteRMI final : public BaseCommandExecute
{
public:
	std::shared_ptr<Peer> peer;
	std::vector<uint8_t> binaryData;
	std::shared_ptr<void> objectPtr;
	std::shared_ptr<rmi::MethodInvocationConverter> methodInvoker;
	uint32_t readOffset;
	Flags flags;

	virtual void Execute() override;
};

class ExecuteReturnRC final : public BaseCommandExecute
{
public:
	std::shared_ptr<Peer> peer;
	std::vector<uint8_t> binaryData;
	std::function<void(std::shared_ptr<Peer>, Flags, std::vector<uint8_t> &,
					   uint32_t)>
		function;
	Flags flags;
	uint32_t readOffset;

	virtual void Execute() override;
};

class ExecuteConnect final : public BaseCommandExecute
{
public:
	std::shared_ptr<Host> host;
	ENetAddress address;

	std::shared_ptr<CommandExecutionQueue> executionQueue;
	ExecuteOnPeer onConnected;

	virtual void Execute() override;
};

class ExecuteSend final : public BaseCommandExecute
{
public:
	std::vector<uint8_t> data;
	std::shared_ptr<Peer> peer;
	Flags flags;

	virtual void Execute() override;
};

class ExecuteDisconnect final : public BaseCommandExecute
{
public:
	std::shared_ptr<Peer> peer;
	uint32_t disconnectData;

	virtual void Execute() override;
};

class ExecuteFunctionPointer final : public BaseCommandExecute
{
public:
	void (*function)();

	virtual void Execute() override;
};

class ExecuteFunctionObject final : public BaseCommandExecute
{
public:
	std::function<void()> function;

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
		commands::ExecuteFunctionObjectOnPeer executeFunctionObjectOnPeer;
		commands::ExecuteFunctionObjectNoArgsOnPeer
			executeFunctionObjectNoArgsOnPeer;
		commands::ExecuteRPC executeRPC;
		commands::ExecuteRMI executeRMI;
		commands::ExecuteReturnRC executeReturnRC;
		commands::ExecuteConnect executeConnect;
		commands::ExecuteSend executeSend;
		commands::ExecuteDisconnect executeDisconnect;
		commands::ExecuteFunctionPointer executeFunctionPointer;
		commands::ExecuteFunctionObject executeFunctionObject;
	};

	bool hasValue;

	void Execute();

public:
	Command(commands::ExecuteOnPeer &&executeOnPeer)
		: executeOnPeer(), hasValue(true)
	{
		this->executeOnPeer = std::move(executeOnPeer);
	}
	Command(commands::ExecuteFunctionObjectOnPeer &&executeFunctionObjectOnPeer)
		: executeFunctionObjectOnPeer(), hasValue(true)
	{
		this->executeFunctionObjectOnPeer =
			std::move(executeFunctionObjectOnPeer);
	}
	Command(commands::ExecuteFunctionObjectNoArgsOnPeer
				&&executeFunctionObjectNoArgsOnPeer)
		: executeFunctionObjectNoArgsOnPeer(), hasValue(true)
	{
		this->executeFunctionObjectNoArgsOnPeer =
			std::move(executeFunctionObjectNoArgsOnPeer);
	}
	Command(commands::ExecuteRPC &&executeRPC) : executeRPC(), hasValue(true)
	{
		this->executeRPC = std::move(executeRPC);
	}
	Command(commands::ExecuteRMI &&executeRMI) : executeRMI(), hasValue(true)
	{
		this->executeRMI = std::move(executeRMI);
	}
	Command(commands::ExecuteReturnRC &&executeReturnRC)
		: executeReturnRC(), hasValue(true)
	{
		this->executeReturnRC = std::move(executeReturnRC);
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
	Command(commands::ExecuteFunctionObject &&executeFunctionObject)
		: executeFunctionObject(), hasValue(true)
	{
		this->executeFunctionObject = std::move(executeFunctionObject);
	}
};
} // namespace icon6

#endif
