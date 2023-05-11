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
	
	class Writer;
	
	namespace impl {
		class BitsWriter {
		public:
			
			inline BitsWriter(std::vector<uint8_t>& buffer);
			inline ~BitsWriter();
			
			inline Writer& ignore(uint32_t bits);
			inline Writer& ignore_byte_perfect();
			inline Writer& reserve_bits(uint32_t bits);
			
			inline Writer& op(const std::string& str);
			inline Writer& op(const std::string_view str);
			inline Writer& op(const std::vector<uint8_t>& binary);
			// constant size byte array
			inline Writer& op(const uint8_t* data, const uint32_t bytes);
			
			inline Writer& op(uint8_t v,  uint32_t bits) { return push_bits(v, bits); }
			inline Writer& op(uint16_t v, uint32_t bits) { return push_bits(v, bits); }
			inline Writer& op(uint32_t v, uint32_t bits) { return push_bits(v, bits); }
			inline Writer& op(uint64_t v, uint32_t bits) { return push_bits(v, bits); }
			inline Writer& op(int8_t v,   uint32_t bits) { return push_bits(v, bits); }
			inline Writer& op(int16_t v,  uint32_t bits) { return push_bits(v, bits); }
			inline Writer& op(int32_t v,  uint32_t bits) { return push_bits(v, bits); }
			inline Writer& op(int64_t v,  uint32_t bits) { return push_bits(v, bits); }
			
			inline Writer& push_bits(uint8_t v, uint32_t bits);
			inline Writer& push_bits(uint16_t v, uint32_t bits);
			inline Writer& push_bits(uint32_t v, uint32_t bits);
			inline Writer& push_bits(uint64_t v, uint32_t bits);
			inline Writer& push_bits(int8_t v, uint32_t bits);
			inline Writer& push_bits(int16_t v, uint32_t bits);
			inline Writer& push_bits(int32_t v, uint32_t bits);
			inline Writer& push_bits(int64_t v, uint32_t bits);
			
		private:
			
			std::vector<uint8_t>& buffer;
			uint32_t byteOffset;
			uint32_t bitOffset;
		};
	} // namespace impl
	
	class Writer : public impl::BitsWriter {
	public:
		
		inline Writer(std::vector<uint8_t>& buffer) : BitsWriter(buffer) {}
		inline ~Writer();
		
		
		
		
		inline Writer& op(float value) { return op(*(uint32_t*)&value, 32); }
		inline Writer& op(double value) { return op(*(uint64_t*)&value, 64); }
		
		inline Writer& op(float value, float min, float max, uint32_t bits);
		inline Writer& op(double value, double min, double max, uint32_t bits);
		
		inline Writer& op(float value, float offset, float min, float max, uint32_t bits);
		inline Writer& op(double value, double offset, double min, double max, uint32_t bits);
		
	public: // array types serialization
		
		// const size array
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
		
		inline Writer& op(int8_t v)   { return op(v,  8); }
		inline Writer& op(int16_t v)  { return op(v, 16); }
		inline Writer& op(int32_t v)  { return op(v, 32); }
		inline Writer& op(int64_t v)  { return op(v, 64); }
		inline Writer& op(uint8_t v)  { return op(v,  8); }
		inline Writer& op(uint16_t v) { return op(v, 16); }
		inline Writer& op(uint32_t v) { return op(v, 32); }
		inline Writer& op(uint64_t v) { return op(v, 64); }
		
		inline Writer& op(uint8_t v,   uint8_t min, uint32_t bits) { return push_bits(v-min, bits); }
		inline Writer& op(uint16_t v, uint16_t min, uint32_t bits) { return push_bits(v-min, bits); }
		inline Writer& op(uint32_t v, uint32_t min, uint32_t bits) { return push_bits(v-min, bits); }
		inline Writer& op(uint64_t v, uint64_t min, uint32_t bits) { return push_bits(v-min, bits); }
		inline Writer& op(int8_t v,    uint8_t min, uint32_t bits) { return push_bits(v-min, bits); }
		inline Writer& op(int16_t v,  uint16_t min, uint32_t bits) { return push_bits(v-min, bits); }
		inline Writer& op(int32_t v,  uint32_t min, uint32_t bits) { return push_bits(v-min, bits); }
		inline Writer& op(int64_t v,  uint64_t min, uint32_t bits) { return push_bits(v-min, bits); }
		
		inline Writer& op(uint8_t v,  uint32_t bits) { return push_bits(v, bits); }
		inline Writer& op(uint16_t v, uint32_t bits) { return push_bits(v, bits); }
		inline Writer& op(uint32_t v, uint32_t bits) { return push_bits(v, bits); }
		inline Writer& op(uint64_t v, uint32_t bits) { return push_bits(v, bits); }
		inline Writer& op(int8_t v,   uint32_t bits) { return push_bits(v, bits); }
		inline Writer& op(int16_t v,  uint32_t bits) { return push_bits(v, bits); }
		inline Writer& op(int32_t v,  uint32_t bits) { return push_bits(v, bits); }
		inline Writer& op(int64_t v,  uint32_t bits) { return push_bits(v, bits); }
	};
	
	
	
	
	inline Writer& Writer::op(float value, float min, float max, uint32_t bits) {
		float fmask = (((uint64_t)1)<<(bits-1ll))-1ll;
		float pv = value * fmask / (max-min);
		uint32_t v = pv+0.4f;
		return push_bits(v, bits);
	}
	
	inline Writer& Writer::op(double value, double min, double max, uint32_t bits) {
		float fmask = (((uint64_t)1)<<(bits-1ll))-1ll;
		float pv = value * fmask / (max-min);
		uint32_t v = pv+0.5;
		return push_bits(v, bits);
	}
	
	
	inline Writer& Writer::op(float value, float offset, float min, float max, uint32_t bits) {
		return op(value + offset, min, max, bits);
	}
	
	inline Writer& Writer::op(double value, double offset, double min, double max, uint32_t bits) {
		return op(value + offset, min, max, bits);
	}
	
	
	
namespace impl {
	inline BitsWriter::BitsWriter(std::vector<uint8_t>& buffer) : buffer(buffer) {
		byteOffset = 0;
		bitOffset = 0;
	}
	
	inline BitsWriter::~BitsWriter() {
		ignore_byte_perfect();
		buffer.resize(byteOffset);
	}
	
	
	
	inline Writer& BitsWriter::op(const std::string& str) {
		return op((const uint8_t*)str.data(), str.size()+1);
	}
	
	inline Writer& BitsWriter::op(const std::string_view str) {
		return op((const uint8_t*)str.data(), str.size()+1);
	}
	
	inline Writer& BitsWriter::op(const std::vector<uint8_t>& binary) {
		return op(binary.data(), binary.size());
	}
	
	inline Writer& BitsWriter::op(const uint8_t* data, const uint32_t bytes) {
		ignore_byte_perfect();
		buffer.insert(buffer.begin()+byteOffset, data, data+bytes);
		byteOffset += bytes;
		return *(Writer*)this;
	}
	
	
	
	inline Writer& BitsWriter::ignore(uint32_t bits) {
		bitOffset += bits;
		byteOffset += bitOffset>>3;
		bitOffset &= 7;
		buffer.resize(byteOffset + ((7+bitOffset)>>3));
		return *(Writer*)this;
	}
	
	inline Writer& BitsWriter::ignore_byte_perfect() {
		if((bitOffset & 7) > 0)
			ignore(8 - (bitOffset & 7));
		return *(Writer*)this;
	}
	
	inline Writer& BitsWriter::reserve_bits(uint32_t bits) {
		uint32_t bytes = byteOffset + ((bitOffset+bits+7)>>3);
		buffer.resize(bytes);
		return *(Writer*)this;
	}
	
	
	
	inline Writer& BitsWriter::push_bits(int8_t v,  uint32_t bits) { return push_bits((uint8_t)v, bits); }
	inline Writer& BitsWriter::push_bits(int16_t v, uint32_t bits) { return push_bits((uint16_t)v, bits); }
	inline Writer& BitsWriter::push_bits(int32_t v, uint32_t bits) { return push_bits((uint32_t)v, bits); }
	inline Writer& BitsWriter::push_bits(int64_t v, uint32_t bits) { return push_bits((uint64_t)v, bits); }
	
	inline Writer& BitsWriter::push_bits(uint8_t v, uint32_t bits) { return push_bits((uint64_t)v, bits); }
	inline Writer& BitsWriter::push_bits(uint16_t v, uint32_t bits) { return push_bits((uint64_t)v, bits); }
	inline Writer& BitsWriter::push_bits(uint32_t v, uint32_t bits) { return push_bits((uint64_t)v, bits); }
	
	
	
	inline Writer& BitsWriter::push_bits(uint64_t v, uint32_t bits) {
		reserve_bits(bits);
		uint32_t dstsize = (8-bitOffset) & 7;
		if(dstsize) {
			dstsize = std::min(dstsize, bits);
			bits -= dstsize;
			
			uint8_t mask = 1 << (dstsize-1);
			buffer[byteOffset] |= (v&mask) << bitOffset;
		}
		
		uint32_t off = dstsize;
		uint32_t fullBytes = (bits-off)>>3;
		off -= 8;
		switch(fullBytes) {
			case 8:
				buffer[++byteOffset] = v>>((off+=8));
				buffer[++byteOffset] = v>>((off+=8));
				buffer[++byteOffset] = v>>((off+=8));
				buffer[++byteOffset] = v>>((off+=8));
				buffer[++byteOffset] = v>>((off+=8));
				buffer[++byteOffset] = v>>((off+=8));
				buffer[++byteOffset] = v>>((off+=8));
				buffer[++byteOffset] = v>>((off+=8));
				break;
			case 7:
				buffer[++byteOffset] = v>>((off+=8));
			case 6:
				buffer[++byteOffset] = v>>((off+=8));
			case 5:
				buffer[++byteOffset] = v>>((off+=8));
			case 4:
				buffer[++byteOffset] = v>>((off+=8));
			case 3:
				buffer[++byteOffset] = v>>((off+=8));
			case 2:
				buffer[++byteOffset] = v>>((off+=8));
			case 1:
				buffer[++byteOffset] = v>>((off+=8));
			case 0:
				if(bits - off) {
					bitOffset = bits-off;
					uint8_t mask = (1<<bitOffset)-1;
					buffer[byteOffset] = (v>>off) & mask;
				}
		}
		return *(Writer*)this;
	}
	
} // namespace impl
} // namespace icon6

#endif

