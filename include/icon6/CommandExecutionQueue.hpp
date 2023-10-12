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

#include "Command.hpp"

namespace icon6 {
	class CommandExecutionQueue:
		std::enable_shared_from_this<CommandExecutionQueue> {
	public:
		
		CommandExecutionQueue();
		~CommandExecutionQueue();
		
		void EnqueueCommand(Command&& command);
		void TryDequeueBulkAny(std::vector<Command>& commands);
		
		union {
			void *__concurrentQueueVoid;
#ifdef ICON6_COMMAND_EXECUTION_QUEUE_CPP_IMPLEMENTATION
			moodycamel::ConcurrentQueue<Command> *concurrentQueueCommands;
#endif
		};
	};
}

#endif

