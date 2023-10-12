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

#include "../bitscpp/include/bitscpp/ByteReader.hpp"

// #include "../include/icon6/Host.hpp"
// #include "../include/icon6/Peer.hpp"
#include "../include/icon6/MessagePassingEnvironment.hpp"
#include "../include/icon6/MethodInvocationEnvironment.hpp"
#include "../include/icon6/CommandExecutionQueue.hpp"

#include "../include/icon6/Command.hpp"

namespace icon6 {
namespace commands {
	void ExecuteOnPeer::Execute()
	{
		function(peer, data, customSharedData);
	}
	
	void ExecuteRPC::Execute()
	{
		bitscpp::ByteReader<true> reader(binaryData.data()+readOffset,
				binaryData.size()-readOffset);
		messageConverter->Call(peer.get(), reader, flags);
	}
	
	void ExecuteRMI::Execute()
	{
		bitscpp::ByteReader<true> reader(binaryData.data()+readOffset,
				binaryData.size()-readOffset);
		methodInvoker->Call(objectPtr, peer.get(), reader, flags);
	}
	
	void ExecuteConnect::Execute()
	{
		onConnected.peer = host->_InternalConnect(address);
		if(executionQueue)
			executionQueue->EnqueueCommand(Command(std::move(onConnected)));
		else
			onConnected.Execute();
	}
	
	void ExecuteSend::Execute()
	{
		peer->_InternalSend(data, flags);
	}
	
	void ExecuteDisconnect::Execute()
	{
		peer->_InternalDisconnect(disconnectData);
	}
	
	void ExecuteFunctionPointer::Execute()
	{
		function();
	}
}

	using namespace commands;

	Command::Command() : hasValue(false)
	{
	}
	
	Command::Command(Command&& other)
	{
		this->hasValue = false;
		(*this) = std::move(other);
	}

	Command& Command::operator=(Command&& other)
	{
		if(hasValue)
			this->~Command();
		struct _CopyTypeStruct {
			uint8_t b[sizeof(Command)];
		};
		*(_CopyTypeStruct*)this = *(_CopyTypeStruct*)&other;
		other.hasValue = false;
		return *this;
	}
	
	Command::~Command()
	{
		if(hasValue) {
			((BaseCommandExecute*)(&executeOnPeer))->~BaseCommandExecute();
			hasValue = false;
		}
	}
	
	void Command::Execute()
	{
		((BaseCommandExecute*)(&executeOnPeer))->Execute();
	}
}
