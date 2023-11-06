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

#include <chrono>

#include "../concurrentqueue/concurrentqueue.h"

#include "../include/icon6/Peer.hpp"
#include "../include/icon6/MessagePassingEnvironment.hpp"
#include "../include/icon6/MethodInvocationEnvironment.hpp"

#include "../include/icon6/CommandExecutionQueue.hpp"

namespace icon6
{
using QueueType = moodycamel::ConcurrentQueue<Command>;

CommandExecutionQueue::CommandExecutionQueue()
{
	asyncExecutionFlags = STOPPED;
	concurrentQueueCommands = new QueueType();
}

CommandExecutionQueue::~CommandExecutionQueue()
{
	WaitStopAsyncExecution();
	delete (QueueType *)concurrentQueueCommands;
	concurrentQueueCommands = nullptr;
}

void CommandExecutionQueue::EnqueueCommand(Command &&command)
{
	((QueueType *)concurrentQueueCommands)->enqueue(std::move(command));
}

uint32_t CommandExecutionQueue::TryDequeueBulkAny(Command *commands,
												  uint32_t max)
{
	size_t size = ((QueueType *)concurrentQueueCommands)->size_approx();
	if (size == 0)
		size = 1;
	size = std::min<size_t>(size, max);
	size_t dequeued = ((QueueType *)concurrentQueueCommands)
						  ->try_dequeue_bulk(commands, size);
	return dequeued;
}

void CommandExecutionQueue::QueueStopAsyncExecution()
{
	asyncExecutionFlags |= QUEUE_STOP;
}

void CommandExecutionQueue::WaitStopAsyncExecution()
{
	QueueStopAsyncExecution();
	while (IsRunningAsync()) {
		std::this_thread::sleep_for(std::chrono::microseconds(10));
	}
}

bool CommandExecutionQueue::IsRunningAsync() const
{
	return asyncExecutionFlags.load() & IS_RUNNING;
}

void CommandExecutionQueue::RunAsyncExecution(
	CommandExecutionQueue *queue, uint32_t sleepMicrosecondsOnNoActions)
{
	std::thread(&CommandExecutionQueue::_InternalExecuteLoop, queue,
				sleepMicrosecondsOnNoActions)
		.detach();
}

void CommandExecutionQueue::_InternalExecuteLoop(
	CommandExecutionQueue *queue, uint32_t sleepMicrosecondsOnNoActions)
{
	queue->asyncExecutionFlags = IS_RUNNING;
	const uint32_t MAX_DEQUEUE_COMMANDS = 128;
	Command commands[MAX_DEQUEUE_COMMANDS];
	while (queue->asyncExecutionFlags.load() == IS_RUNNING) {
		const uint32_t dequeued =
			queue->TryDequeueBulkAny(commands, MAX_DEQUEUE_COMMANDS);
		if (dequeued > 0) {
			for (uint32_t i = 0; i < dequeued; ++i) {
				commands[i].Execute();
				commands[i].~Command();
			}
		} else {
			std::this_thread::sleep_for(
				std::chrono::microseconds(sleepMicrosecondsOnNoActions));
		}
	}
	queue->asyncExecutionFlags = STOPPED;
}
} // namespace icon6
