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

#ifndef ICON6_METHOD_INVOCATION_CONVERTER_HPP
#define ICON6_METHOD_INVOCATION_CONVERTER_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <bitscpp/ByteWriterExtensions.hpp>
#include <bitscpp/ByteReaderExtensions.hpp>

#include "Host.hpp"
#include "Peer.hpp"

namespace icon6 {
	
	class CommandExecutionQueue;
	
namespace rmi {
	
	class Class;
	
	class MethodInvocationConverter {
	public:
		virtual ~MethodInvocationConverter() = default;
		
		virtual void Call(std::shared_ptr<void> objectPtr,
				Peer* peer, bitscpp::ByteReader<true>& reader,
				uint32_t flags) = 0;
		
		std::shared_ptr<CommandExecutionQueue> executionQueue;
	};
	
	template<typename Tclass, typename Targ>
	class MessageNetworkAwareMethodInvocationConverterSpec : public MethodInvocationConverter {
	public:
		MessageNetworkAwareMethodInvocationConverterSpec(
				Class* _class,
				void(Tclass::*memberFunction)(Peer* peer, uint32_t flags, Targ&& message)) :
				onReceive(memberFunction), _class(_class)
		{
		}
		
		virtual ~MessageNetworkAwareMethodInvocationConverterSpec() = default;
		
		virtual void Call(std::shared_ptr<void> objectPtr,
				Peer* peer, bitscpp::ByteReader<true>& reader,
				uint32_t flags) override
		{
			std::shared_ptr<Tclass> ptr = std::static_pointer_cast<Tclass>(objectPtr);
			Targ message;
			reader.op(message);
			(ptr.get()->*onReceive)(peer, flags, std::move(message));
		}
		
	private:
		
		void(Tclass::*onReceive)(Peer* peer, uint32_t flags, Targ&& message);
		class Class* _class;
	};

}
}

#endif

