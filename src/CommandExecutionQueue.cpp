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

#include "../concurrentqueue/concurrentqueue.h"

#include "../../bitscpp/include/bitscpp/ByteReader.hpp"

#include "../include/icon6/Peer.hpp"
#include "../include/icon6/MessagePassingEnvironment.hpp"
#include "../include/icon6/MethodInvocationEnvironment.hpp"

#include "../include/icon6/CommandExecutionQueue.hpp"

namespace icon6 {
	using QueueType = moodycamel::ConcurrentQueue<Command>;
	
	CommandExecutionQueue::CommandExecutionQueue()
	{
		concurrentQueueCommands = new QueueType();
	}
	
	CommandExecutionQueue::~CommandExecutionQueue()
	{
		delete (QueueType*)concurrentQueueCommands;
		concurrentQueueCommands = nullptr;
	}

	void CommandExecutionQueue::EnqueueCommand(Command&& command)
	{
		((QueueType*)concurrentQueueCommands)->enqueue(std::move(command));
	}
	
	void CommandExecutionQueue::TryDequeueBulkAny(std::vector<Command>& commands)
	{
		commands.clear();
		size_t size = ((QueueType*)concurrentQueueCommands)->size_approx();
		if(size == 0)
			size = 1;
		commands.resize(size);
		size_t nextSize = ((QueueType*)concurrentQueueCommands)
			->try_dequeue_bulk(commands.data(), size);
		commands.resize(nextSize);
	}
}

