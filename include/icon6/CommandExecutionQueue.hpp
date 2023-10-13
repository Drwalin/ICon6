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

#ifndef ICON6_COMMAND_EXECUTION_QUEUE_HPP
#define ICON6_COMMAND_EXECUTION_QUEUE_HPP

#include <memory>
#include <vector>
#include <atomic>

#include "Command.hpp"

namespace icon6 {
	class CommandExecutionQueue:
		std::enable_shared_from_this<CommandExecutionQueue> {
	public:
		
		CommandExecutionQueue();
		~CommandExecutionQueue();
		
		void EnqueueCommand(Command&& command);
		void TryDequeueBulkAny(std::vector<Command>& commands);
		
		static void RunAsyncExecution(
				std::shared_ptr<CommandExecutionQueue> queue,
				uint32_t sleepMicrosecondsOnNoActions);
		void QueueStopAsyncExecution();
		void WaitStopAsyncExecution();
		bool IsRunningAsync() const;
		
	private:
		
		enum AsyncExecutionFlags {
			STARTING_RUNNING = 1,
			IS_RUNNING = 2,
			QUEUE_STOP = 4,
			STOPPED = 0
		};
		
		static void _InternalExecuteLoop(
				std::shared_ptr<CommandExecutionQueue> queue,
				uint32_t sleepMicrosecondsOnNoActions);
		std::atomic<uint32_t> asyncExecutionFlags;
		
	private:
		void *concurrentQueueCommands;
	};
}

#endif

