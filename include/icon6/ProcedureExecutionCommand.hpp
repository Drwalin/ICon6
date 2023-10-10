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

#ifndef ICON6_PROCEDURE_EXECUTION_COMMAND_HPP
#define ICON6_PROCEDURE_EXECUTION_COMMAND_HPP

#include <cinttypes>

#include <string>
#include <memory>
#include <vector>

namespace icon6 {
	
	class Peer;
	
	class MessageConverter;
	namespace rmi {
		class MethodInvokeConverter;
	}

	class ProcedureExecutionCommand {
	public:
		
		ProcedureExecutionCommand(ProcedureExecutionCommand&&) = default;
		ProcedureExecutionCommand(const ProcedureExecutionCommand&) = default;
		ProcedureExecutionCommand() = default;
		~ProcedureExecutionCommand();
		
		
		ProcedureExecutionCommand& operator=(ProcedureExecutionCommand&&) = default;
		
		ProcedureExecutionCommand(
				Peer *peer,
				uint32_t flags,
				std::vector<uint8_t> &binaryData,
				size_t readOffset,
				std::shared_ptr<MessageConverter> messageConverter);
		
		ProcedureExecutionCommand(
				Peer *peer,
				uint32_t flags,
				std::vector<uint8_t> &binaryData,
				size_t readOffset,
				std::shared_ptr<void> objectPtr,
				std::shared_ptr<rmi::MethodInvokeConverter> methodInvoker);
		
		void Execute();
		
		std::shared_ptr<Peer> peer;
		uint32_t flags;
		std::vector<uint8_t> binaryData;
		size_t readOffset;
		
		std::shared_ptr<void> objectPtr;
		
		std::shared_ptr<void> messageOrMethodConverter;
	};
	
	class ProcedureExecutionQueue :
		std::enable_shared_from_this<ProcedureExecutionQueue> {
	public:
		
		ProcedureExecutionQueue();
		~ProcedureExecutionQueue();
		
		void EnqueueCall(ProcedureExecutionCommand&& command);
		void TryDequeueBulkAny(std::vector<ProcedureExecutionCommand>& commands);
		
		union {
			void *__concurrentQueueVoid;
#ifdef ICON6_PROCEDURE_EXECUTION_COMMAND_CPP_IMPLEMENTATION
			moodycamel::ConcurrentQueue<ProcedureExecutionCommand> *concurrentQueueCommands;
#endif
		};
	};
}


#endif

