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

#pragma once

#ifndef ICON6_SERIALIZER_IMPL_HPP
#define ICON6_SERIALIZER_IMPL_HPP

#include "Serializer.hpp"

namespace icon6 {
	
	inline Serializer::Serializer(std::vector<uint8_t>& buffer) {
	}
	
	inline Serializer::Serializer(const uint8_t* const data, uint32_t size) {
	}
	
	
	inline Serializer& Serializer::op(std::string& str) {
		return *this;
	}
	inline Serializer& Serializer::op(std::vector<uint8_t>& binary) {
		return *this;
	}
	
	
	
	inline Serializer& Serializer::op(uint8_t& value, uint32_t bits) {
		return opGenericInteger(value, bits);
	}
	inline Serializer& Serializer::op(uint16_t& value, uint32_t bits) {
		return opGenericInteger(value, bits);
	}
	inline Serializer& Serializer::op(uint32_t& value, uint32_t bits) {
		return opGenericInteger(value, bits);
	}
	inline Serializer& Serializer::op(uint64_t& value, uint32_t bits) {
		return opGenericInteger(value, bits);
	}
	inline Serializer& Serializer::op(int8_t& value, uint32_t bits) {
		return opGenericInteger(value, bits);
	}
	inline Serializer& Serializer::op(int16_t& value, uint32_t bits) {
		return opGenericInteger(value, bits);
	}
	inline Serializer& Serializer::op(int32_t& value, uint32_t bits) {
		return opGenericInteger(value, bits);
	}
	inline Serializer& Serializer::op(int64_t& value, uint32_t bits) {
		return opGenericInteger(value, bits);
	}
	
	
	inline Serializer& Serializer::op(float& value, uint32_t bits) {
		return op(value, 0, 0, bits);
	}
	inline Serializer& Serializer::op(double& value, uint32_t bits) {
		return op(value, 0, 0, bits);
	}
	inline Serializer& Serializer::op(long double& value, uint32_t bits) {
		return op(value, 0, 0, bits);
	}
	
	inline Serializer& Serializer::op(float& value, float min, float max,
			uint32_t bits) {
		return opGenericFloat(value, min, max, bits);
	}
	inline Serializer& Serializer::op(double& value, double min, double max,
			uint32_t bits) {
		return opGenericFloat(value, min, max, bits);
	}
	inline Serializer& Serializer::op(long double& value, long double min,
			long double max, uint32_t bits) {
		return opGenericFloat(value, min, max, bits);
	}
}

#endif

