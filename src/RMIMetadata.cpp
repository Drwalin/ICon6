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

#include "../include/icon6/MethodInvocationConverter.hpp"
#include "../include/icon6/RMIMetadata.hpp"

namespace icon6
{
namespace rmi
{
Class::Class(Class *parentClass, std::string name, void *(*constructor)(),
			 void (*const destructor)(void *))
	: constructor(constructor), destructor(destructor),
	  parentClass(parentClass), name(name)
{
	if (parentClass) {
		parentClass->inheritedClasses.insert(this);
		Class *parenting = parentClass;
		while (parenting) {
			for (auto mtd : parenting->methods) {
				if (methods.count(mtd.first) == 0) {
					methods[mtd.first] = mtd.second;
				}
			}
			parenting = parenting->parentClass;
		}
	}
}

void Class::RegisterMethod(std::string methodName,
						   MethodInvocationConverter *converter)
{
	methods[methodName] = converter;
	for (Class *cls : inheritedClasses) {
		if (cls->methods.count(methodName) == 0) {
			cls->RegisterMethod(methodName, converter);
		}
	}
}

Object::Object() : objectPtr(nullptr), objectClass(nullptr) {}
Object::Object(void *objectPtr, Class *objectClass)
	: objectPtr(objectPtr), objectClass(objectClass)
{
}

Object::~Object()
{
	if (objectPtr) {
		objectClass->destructor(objectPtr);
		objectPtr = nullptr;
	}
}

Object::Object(Object &&o)
{
	objectPtr = o.objectPtr;
	objectClass = o.objectClass;
	o.objectPtr = nullptr;
	o.objectClass = nullptr;
}

Object &Object::operator=(Object &&o)
{
	objectPtr = o.objectPtr;
	objectClass = o.objectClass;
	o.objectPtr = nullptr;
	o.objectClass = nullptr;
	return *this;
}
} // namespace rmi
} // namespace icon6
