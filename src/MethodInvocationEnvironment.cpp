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

#include "../include/icon6/ProcedureExecutionCommand.hpp"

#include "../include/icon6/MethodInvocationEnvironment.hpp"

namespace icon6 {
namespace rmi {
	Class::Class(std::shared_ptr<Class> parentClass, std::string name,
			std::shared_ptr<void> (*constructor)()) :
		constructor(constructor), parentClass(parentClass.get()), name(name)
	{
		if(parentClass) {
			parentClass->inheritedClasses.insert(this);
			Class* parenting = parentClass.get();
			while(parenting) {
				for(auto mtd : parenting->methods) {
					if(methods.count(mtd.first) == 0) {
						methods[mtd.first] = mtd.second;
					}
				}
				parenting = parenting->parentClass;
			}
		}
	}

	void Class::RegisterMethod(std::string methodName,
						  std::shared_ptr<MethodInvocationConverter> converter)
	{
		methods[methodName] = converter;
		for(Class* cls : inheritedClasses) {
			if(cls->methods.count(methodName) == 0) {
				cls->RegisterMethod(methodName, converter);
			}
		}
	}
	
	
	
	std::shared_ptr<Class> MethodInvocationEnvironment::GetClassByName(
			std::string name)
	{
		auto it = classes.find(name);
		if(it == classes.end())
			return nullptr;
		return it->second;
	}

	void MethodInvocationEnvironment::OnReceive(Peer* peer,
			std::vector<uint8_t>& data, uint32_t flags) {
		if(data[0] == 0) {
			bitscpp::ByteReader reader(data.data()+1, data.size()-1);
			
			uint64_t objectId = 0;
			reader.op(objectId);
			auto object = objects.find(objectId);
			if(object != objects.end()) {
				std::string name;
				reader.op(name);
				auto cls = object->second.obejctClass;
				auto method = cls->methods.find(name);
				if(method != cls->methods.end()) {
					auto mtd = method->second;
					if(mtd->executionQueue) {
						mtd->executionQueue->EnqueueCall(
							ProcedureExecutionCommand(
									peer,
									flags,
									data,
									reader.get_offset(),
									object->second.objectPtr,
									mtd));
					} else {
						mtd->Call(object->second.objectPtr, peer, reader, flags);
					}
				} else { // method name does not exists
					// TODO: do something, show error or anything
				}
			} else { // this object does not exists locally
				// TODO: do something, show error or anything
			}
		} else {
			MessagePassingEnvironment::OnReceive(peer, data, flags);
		}
	}
	
}
}

