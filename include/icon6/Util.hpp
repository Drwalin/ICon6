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

#ifndef ICON6_UTIL_HPP
#define ICON6_UTIL_HPP

#include <memory>

namespace icon6
{

template <typename... Args>
auto ConvertLambdaToFunctionPtr(void (*fun)(Args...))
{
	return fun;
}

template <typename Fun> auto ConvertLambdaToFunctionPtr(Fun &&fun)
{
	return +fun;
}
} // namespace icon6

#endif
