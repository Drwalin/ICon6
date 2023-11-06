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

#ifndef ICON6_FLAGS_HPP
#define ICON6_FLAGS_HPP

#include <cinttypes>

#include <steam/steamnetworkingtypes.h>

namespace icon6
{
struct Flags {
	uint32_t field;
	void GetNetworkOrder(void *ptr) const;

	Flags(uint32_t v) : field(v) {}

	Flags() : field(0) {}
	Flags(Flags &&) = default;
	Flags(Flags &) = default;
	Flags(const Flags &) = default;

	Flags &operator=(Flags &&) = default;
	Flags &operator=(Flags &) = default;
	Flags &operator=(const Flags &) = default;

	inline Flags operator&(const Flags o) const { return field & o.field; }
	inline Flags operator^(const Flags o) const { return field ^ o.field; }
	inline Flags operator|(const Flags o) const { return field | o.field; }

	inline Flags operator&=(const Flags o) { return *this = *this & o; }
	inline Flags operator^=(const Flags o) { return *this = *this ^ o; }
	inline Flags operator|=(const Flags o) { return *this = *this | o; }

	inline bool operator==(const Flags o) const { return field == o.field; }
	inline bool operator!=(const Flags o) const { return field != o.field; }
	inline operator bool() const { return field; }
};

inline const static Flags FLAG_UNRELIABLE = k_nSteamNetworkingSend_Unreliable;
inline const static Flags FLAG_RELIABLE = k_nSteamNetworkingSend_Reliable;
inline const static Flags FLAG_NO_NAGLE = k_nSteamNetworkingSend_NoNagle;

inline const static Flags FLAG_UNRELIABLE_NO_NAGLE =
	FLAG_UNRELIABLE | FLAG_NO_NAGLE;
inline const static Flags FLAG_RELIABLE_NO_NAGLE =
	FLAG_RELIABLE | FLAG_NO_NAGLE;

namespace MethodProtocolSendFlags
{
inline const uint8_t FUNCTION_SEND_PREFIX = 0;
inline const uint8_t FUNCTION_CALL_PREFIX = 1;
inline const uint8_t METHOD_SEND_PREFIX = 2;
inline const uint8_t METHOD_CALL_PREFIX = 3;
inline const uint8_t RETURN_CALLBACK = 4;
} // namespace MethodProtocolSendFlags
} // namespace icon6

#endif
