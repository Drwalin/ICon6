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

#ifndef ICON6_METHOD_INVOCATION_ENVIRONMENT_HPP
#define ICON6_METHOD_INVOCATION_ENVIRONMENT_HPP

#include "MessagePassingEnvironment.hpp"

namespace icon6 {
namespace rmi {
	
	template<typename Tclass, typename Targ>
	class MessageMethodInvocationConverterSpec : public MessageConverter {
	public:
		MessageMethodInvocationConverterSpec(void(*onReceive)(Peer* peer,
			Targ&& message, uint32_t flags), class Class* _class) :
				onReceive(onReceive), _class(_class) {
		}
		
		virtual ~MessageMethodInvocationConverterSpec() = default;
		
		virtual void Call(MessagePassingEnvironment& mpe,
				Peer* peer, bitscpp::ByteReader<true>& reader,
				uint32_t flags) override {
			Targ message;
			reader.op(message);
			onReceive(peer, std::move(message), flags);
		}
		
	private:
		
		void(*const onReceive)(Peer* peer, Targ&& message, uint32_t flags);
		class Class* _class;
	};
	
	
	class Class {
	public:
		
		Class* parentClass;
		std::unordered_set<Class*> inheritedClasses;
		std::string name;
		
		std::unordered_map<std::string, std::shared_ptr<MessageConverter>> methods;
	};
	
	class Object {
	public:
		
		Class* obejctClass;
		std::shared_ptr<void> objectPtr;
	};
	
	
	
	class MethodInvocationEnvironment : public MessagePassingEnvironment {
	public:
		
		
	protected:
		
		std::unordered_map<size_t, std::shared_ptr<void>> objects;
		
	};
	
}
}

#endif

