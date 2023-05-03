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

#ifndef ICON6_SERIALIZER_HPP
#define ICON6_SERIALIZER_HPP

#include <cinttypes>

#include <string>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>

namespace icon6 {
	
	class Serializer {
	public:
		// constructor for serialization
		inline Serializer(std::vector<uint8_t>& buffer);
		// constructor for deserialization
		inline Serializer(const uint8_t* const data, uint32_t size);
		
		inline Serializer& op(std::string& str);
		inline Serializer& op(std::vector<uint8_t>& binary);
		// constant size array
		inline Serializer& op(void* data, const uint32_t bytes);
		
		
		
		template<typename T>
		inline Serializer& opGenericInteger(T& value, uint32_t bits);
		
		template<typename T>
		inline Serializer& opGenericFloat(T& value, T min, T max, uint32_t bits);
		
	public: // array types serialization
		
		// constant size array
		// if min==max then min and max are ignored
		// if bits == sizeof(T)*8 then original data is saved
		template<typename T>
		inline Serializer& op(T* data, const uint32_t elements, T min,
				T max, uint32_t bits);
		
		template<typename T>
		inline Serializer& op(std::vector<T>& arr, T min, T max, uint32_t bits);
		
		template<typename T>
		inline Serializer& op(std::set<T>& arr, T min, T max, uint32_t bits);
		template<typename T>
		inline Serializer& op(std::unordered_set<T>& arr, T min, T max, uint32_t bits);
		template<typename T1, typename T2>
		inline Serializer& op(std::map<T1, T2>& arr, T1 min1, T1 max1, T2 min2, T2 max2, uint32_t bits);
		template<typename T1, typename T2>
		inline Serializer& op(std::unordered_map<T1, T2>& arr, T1 min1, T1 max1, T2 min2, T2 max2, uint32_t bits);
		
	public: // primitive detailed types serialization
		
		inline Serializer& op(uint8_t& value, uint32_t bits=8);
		inline Serializer& op(uint16_t& value, uint32_t bits=16);
		inline Serializer& op(uint32_t& value, uint32_t bits=32);
		inline Serializer& op(uint64_t& value, uint32_t bits=64);
		inline Serializer& op(int8_t& value, uint32_t bits=8);
		inline Serializer& op(int16_t& value, uint32_t bits=16);
		inline Serializer& op(int32_t& value, uint32_t bits=32);
		inline Serializer& op(int64_t& value, uint32_t bits=64);
		
		inline Serializer& op(float& value, uint32_t bits=32);
		inline Serializer& op(double& value, uint32_t bits=64);
		inline Serializer& op(long double& value, uint32_t bits=80);
		
		inline Serializer& op(float& value, float min, float max,
				uint32_t bits=32);
		inline Serializer& op(double& value, double min, double max,
				uint32_t bits=64);
		inline Serializer& op(long double& value, long double min,
				long double max, uint32_t bits=80);
		
	private:
		
		std::vector<uint8_t>& writeBuffer;
		uint8_t* readWriteBuffer;
		uint32_t bufferCapacity;
		uint32_t bitOffset;
		
		const bool writing;
	};
}

#include "Serializer.impl.hpp"

#endif

