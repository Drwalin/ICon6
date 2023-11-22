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

#include "../include/icon6/MessagePassingEnvironment.hpp"
#include "../include/icon6/MethodInvocationEnvironment.hpp"
#include "../include/icon6/CommandExecutionQueue.hpp"
#include "../include/icon6/PeerGNS.hpp"

#include "../include/icon6/Command.hpp"

namespace icon6
{
namespace commands
{
void ExecuteOnPeer::Execute() { function(peer, data, userPointer); }

void ExecuteOnPeerNoArgs::Execute() { function(peer); }

void ExecuteRPC::Execute() { messageConverter->Call(peer, reader, flags); }

void ExecuteRMI::Execute()
{
	methodInvoker->Call(objectPtr, peer, reader, flags);
}

void ExecuteReturnRC::Execute() { function(peer, flags, reader, funcPtr); }

void ExecuteConnectGNS::Execute()
{
	onConnected.peer = host->_InternalConnect(&address);
	if (executionQueue) {
		executionQueue->EnqueueCommand(Command(std::move(onConnected)));
	} else {
		onConnected.Execute();
	}
}

void ExecuteSend::Execute() { peer->_InternalSendOrQueue(data, flags); }

void ExecuteDisconnect::Execute() { peer->_InternalDisconnect(); }

void ExecuteFunctionPointer::Execute() { function(); }
} // namespace commands

using namespace commands;

void Command::Execute()
{
	std::visit(
		[](auto &&c) -> int {
			if constexpr (std::is_same_v<std::decay_t<decltype(c)>, int> ==
						  false)
				c.Execute();
			return 0;
		},
		std::move(cmd));
	cmd = 0;
}
} // namespace icon6
