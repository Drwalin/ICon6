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

#ifndef ICON6_WRITER_HPP
#define ICON6_WRITER_HPP

#include <cinttypes>

#include <string>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <functional>

namespace icon6 {
	
	class Writer {
	public:
		inline Writer(std::vector<uint8_t>& buffer);
		inline ~Writer();
		
		
		inline Writer& op(const std::string_view str);
		inline Writer& op(const std::vector<uint8_t>& binary);
		// constant size byte array
		inline Writer& op(const uint8_t* data, const uint32_t bytes);
		
		
		inline Writer& ignore(uint32_t bits);
		inline Writer& ignore_byte_perfect();
		inline Writer& reserve_bits(uint32_t bits);
		
		inline Writer& op(uint64_t value, uint32_t bits);
		inline Writer& op(uint64_t value, uint64_t min, uint32_t bits);
		inline Writer& op(int64_t value, uint32_t bits);
		inline Writer& op(int64_t value, int64_t min, uint32_t bits);
		
		
		inline Writer& op(float value) { return op(*(uint32_t*)&value, 32); }
		inline Writer& op(double value) { return op(*(uint64_t*)&value, 64); }
		
		inline Writer& op(float value, float min, float max, uint32_t bits);
		inline Writer& op(double value, double min, double max, uint32_t bits);
		
	public: // array types serialization
		
		template<typename T, typename... Args>
		inline Writer& op(const T* data, const uint32_t elements, Args... args) {
			for(uint32_t i=0; i<elements; ++i)
				op(data[i], args...);
			return *this;
		}
		
		template<typename T, typename... Args>
		inline Writer& op(const std::vector<T>& arr, Args... args) {
			op(arr.size(), 32);
			return op(arr.data(), args...);
		}
		
		
		
		template<typename T, typename... Args>
		inline Writer& op(const std::set<T>& set, Args... args) {
			op(set.size(), 32);
			for(auto& v : set)
				op(v, args...);
			return *this;
		}
		
		template<typename T, typename... Args>
		inline Writer& op(const std::unordered_set<T>& set, Args... args) {
			op(set.size(), 32);
			for(auto& v : set)
				op(v, args...);
			return *this;
		}
		
	public:
		
		inline Writer& op(uint8_t v,  uint32_t bits) { return op((uint64_t)v, bits); }
		inline Writer& op(uint16_t v, uint32_t bits) { return op((uint64_t)v, bits); }
		inline Writer& op(uint32_t v, uint32_t bits) { return op((uint64_t)v, bits); }
		inline Writer& op(int8_t v,  uint32_t bits) { return op((int64_t)v, bits); }
		inline Writer& op(int16_t v, uint32_t bits) { return op((int64_t)v, bits); }
		inline Writer& op(int32_t v, uint32_t bits) { return op((int64_t)v, bits); }
		
		inline Writer& op(uint8_t v,  uint64_t min, uint32_t bits) { return op((uint64_t)v, min, bits); }
		inline Writer& op(uint16_t v, uint64_t min, uint32_t bits) { return op((uint64_t)v, min, bits); }
		inline Writer& op(uint32_t v, uint64_t min, uint32_t bits) { return op((uint64_t)v, min, bits); }
		inline Writer& op(int8_t v,  uint64_t min, uint32_t bits) { return op((int64_t)v, min, bits); }
		inline Writer& op(int16_t v, uint64_t min, uint32_t bits) { return op((int64_t)v, min, bits); }
		inline Writer& op(int32_t v, uint64_t min, uint32_t bits) { return op((int64_t)v, min, bits); }
		
		inline Writer& op(int8_t v) { return op(v, 8); }
		inline Writer& op(int16_t v) { return op(v, 16); }
		inline Writer& op(int32_t v) { return op(v, 32); }
		inline Writer& op(int64_t v) { return op(v, 64); }
		
		inline Writer& op(uint8_t v) { return op(v, 8); }
		inline Writer& op(uint16_t v) { return op(v, 16); }
		inline Writer& op(uint32_t v) { return op(v, 32); }
		inline Writer& op(uint64_t v) { return op(v, 64); }
		
	private:
		
		std::vector<uint8_t>& buffer;
		uint32_t byteOffset;
		uint32_t bitOffset;
	};
	
	
	
	inline Writer::Writer(std::vector<uint8_t>& buffer) : buffer(buffer) {
		byteOffset = 0;
		bitOffset = 0;
	}
	
	inline Writer::~Writer() {
		ignore_byte_perfect();
		buffer.resize(byteOffset);
	}
	
	
	inline Writer& Writer::op(const std::string_view str) {
		return op((const uint8_t*)str.data(), str.size()+1);
	}
	
	inline Writer& Writer::op(const std::vector<uint8_t>& binary) {
		return op(binary.data(), binary.size());
	}
	
	inline Writer& Writer::op(const uint8_t* data, const uint32_t bytes) {
		ignore_byte_perfect();
		buffer.insert(buffer.begin()+byteOffset, data, data+bytes);
		byteOffset += bytes;
		return *this;
	}
	
	
	
	inline Writer& Writer::ignore(uint32_t bits) {
		bitOffset += bits;
		byteOffset += bitOffset>>3;
		bitOffset &= 7;
		buffer.resize(byteOffset + ((7+bitOffset)>>1));
		return *this;
	}
	
	inline Writer& Writer::ignore_byte_perfect() {
		if((bitOffset & 7) > 0)
			ignore(8 - (bitOffset & 7));
		return *this;
	}
	
	inline Writer& Writer::reserve_bits(uint32_t bits) {
		uint32_t bytes = byteOffset + ((bitOffset+bits+7)>>3);
		buffer.resize(bytes);
		return *this;
	}
	
	
	
	inline Writer& Writer::op(uint64_t value, uint64_t min, uint32_t bits) {
		return op(value-min, bits);
	}
	inline Writer& Writer::op(int64_t value, int64_t min, uint32_t bits) {
		return op((uint64_t)(value-min), bits);
	}
	
	
	
	
	
	inline Writer& Writer::op(uint64_t value, uint32_t bits) {
		reserve_bits(bits);
		uint8_t* ptr = buffer.data()+byteOffset + (bitOffset>>3);
		
		
		
		uint32_t bytes = bits>>3;
		for(int i=0; i<bytes; ++i) {
			
		}
		if(bits & 7) {
		}
		
		
		
		bitOffset += bits;
		byteOffset += bitOffset>>3;
		bitOffset &= 7;
		return *this;
	}
	
	inline Writer& Writer::op(int64_t value, uint32_t bits) {
		if(value < 0)
			op(1, 1);
		else
			op(0, 1);
		op(value, bits-1);
		return *this;
	}
	
	
	
	inline Writer& Writer::op(float value, float min, float max, uint32_t bits);
	inline Writer& Writer::op(double value, double min, double max, uint32_t bits);
	
}

#endif

