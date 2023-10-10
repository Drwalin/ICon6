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

#ifdef ICON6_PROCEDURE_EXECUTION_COMMAND_CPP_IMPLEMENTATION
#error "ICon6 src/Host.cpp cannot be included before any indirect or direct inclusion of ICon6 include/icon6/Host.hpp"
#endif

#include "../concurrentqueue/concurrentqueue.h"
#define ICON6_PROCEDURE_EXECUTION_COMMAND_CPP_IMPLEMENTATION

#include "../../bitscpp/include/bitscpp/ByteReader.hpp"

#include "../include/icon6/Peer.hpp"
#include "../include/icon6/MessagePassingEnvironment.hpp"
#include "../include/icon6/MethodInvocationEnvironment.hpp"

#include "../include/icon6/ProcedureExecutionCommand.hpp"

namespace icon6 {
	ProcedureExecutionCommand::~ProcedureExecutionCommand()
	{
	}
	
	ProcedureExecutionCommand::ProcedureExecutionCommand(
			Peer*peer,
			uint32_t flags,
			std::vector<uint8_t>& binaryData,
			size_t readOffset,
			std::shared_ptr<MessageConverter> messageConverter) :
		peer(peer->shared_from_this()),
		flags(flags),
		readOffset(readOffset),
		objectPtr(nullptr),
		messageOrMethodConverter(messageConverter)
	{
		// TODO implement getting vectors from pool
		std::swap(this->binaryData, binaryData);
	}
	
	ProcedureExecutionCommand::ProcedureExecutionCommand(
			Peer*peer,
			uint32_t flags,
			std::vector<uint8_t>& binaryData,
			size_t readOffset,
			std::shared_ptr<void> objectPtr,
			std::shared_ptr<rmi::MethodInvokeConverter> methodInvoker) :
		peer(peer->shared_from_this()),
		flags(flags),
		readOffset(readOffset),
		objectPtr(objectPtr),
		messageOrMethodConverter(methodInvoker)
	{
		// TODO implement getting vectors from pool
		std::swap(this->binaryData, binaryData);
	}
	
	void ProcedureExecutionCommand::Execute()
	{
		if(objectPtr) {
			auto mtd = std::static_pointer_cast<rmi::MethodInvokeConverter>(
					messageOrMethodConverter);
			bitscpp::ByteReader reader(binaryData.data()+readOffset,
					binaryData.size()-readOffset);
			mtd->Call(objectPtr, peer.get(), reader, flags);
		} else {
			auto mtd = std::static_pointer_cast<MessageConverter>(
					messageOrMethodConverter);
			bitscpp::ByteReader reader(binaryData.data()+readOffset,
					binaryData.size()-readOffset);
			mtd->Call(peer.get(), reader, flags);
		}
	}
	
	
	
	ProcedureExecutionQueue::ProcedureExecutionQueue()
	{
		concurrentQueueCommands = new moodycamel::ConcurrentQueue<ProcedureExecutionCommand>();
	}
	
	ProcedureExecutionQueue::~ProcedureExecutionQueue()
	{
		delete concurrentQueueCommands;
		concurrentQueueCommands = nullptr;
	}
	
	void ProcedureExecutionQueue::EnqueueCall(
			ProcedureExecutionCommand&& command)
	{
		concurrentQueueCommands->enqueue(command);
	}
	
	void ProcedureExecutionQueue::TryDequeueBulkAny(
			std::vector<ProcedureExecutionCommand>& commands)
	{
		size_t size = std::max<size_t>(1, concurrentQueueCommands->size_approx());
		commands.resize(size);
		size_t nextSize = concurrentQueueCommands
			->try_dequeue_bulk(commands.data(), size);
		commands.resize(nextSize);
	}
}

