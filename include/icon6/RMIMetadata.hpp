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

#ifndef ICON6_RMI_METADATA_HPP
#define ICON6_RMI_METADATA_HPP

#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace icon6
{
namespace rmi
{

class MethodInvocationConverter;

class Class
{
public:
	Class(Class *parentClass, std::string name,
		  std::shared_ptr<void> (*constructor)());

	void RegisterMethod(std::string methodName,
						std::shared_ptr<MethodInvocationConverter> converter);

	std::unordered_map<std::string, std::shared_ptr<MethodInvocationConverter>>
		methods;
	std::shared_ptr<void> (*const constructor)();

	Class *const parentClass;
	std::unordered_set<Class *> inheritedClasses;
	const std::string name;
};

class Object
{
public:
	std::shared_ptr<void> objectPtr;
	Class *obejctClass;
};

} // namespace rmi
} // namespace icon6

#endif
