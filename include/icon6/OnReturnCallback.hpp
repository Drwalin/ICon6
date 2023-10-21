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

#ifndef ICON6_ON_RETURN_CALLBACK_HPP
#define ICON6_ON_RETURN_CALLBACK_HPP

#include <memory>
#include <functional>

#include "Host.hpp"
#include "Peer.hpp"

namespace icon6
{
class OnReturnCallback
{
public:
	~OnReturnCallback() = default;

	OnReturnCallback() = default;
	OnReturnCallback(OnReturnCallback &&) = default;

	OnReturnCallback(OnReturnCallback &) = delete;
	OnReturnCallback(const OnReturnCallback &) = delete;

	OnReturnCallback &operator=(OnReturnCallback &&) = default;

	OnReturnCallback &operator=(OnReturnCallback &) = delete;
	OnReturnCallback &operator=(const OnReturnCallback &) = delete;

	bool IsExpired(std::chrono::time_point<std::chrono::steady_clock> t =
					   std::chrono::steady_clock::now()) const;
	void Execute(Peer *peer, Flags flags, ByteReader &reader);
	void ExecuteTimeout();

	template <typename Tret, typename Tfunc>
	static OnReturnCallback
	Make(Tfunc &&_onReturnedValue, std::function<void(Peer *)> onTimeout,
		 uint32_t timeoutMilliseconds, Peer *peer,
		 std::shared_ptr<CommandExecutionQueue> executionQueue = nullptr)
	{
		OnReturnCallback ret;
		OnReturnCallback *self = &ret;

		self->onTimeout = onTimeout;
		self->executionQueue = executionQueue;
		self->timeoutTimePoint =
			std::chrono::steady_clock::now() +
			(std::chrono::milliseconds(timeoutMilliseconds));
		self->peer = peer;
		self->onReturnedValue = [_onReturnedValue](Peer *peer, Flags flags,
												   ByteReader &reader) -> void {
			typename std::remove_const<
				typename std::remove_reference<Tret>::type>::type ret;
			reader.op(ret);
			_onReturnedValue(peer, flags, ret);
		};

		return ret;
	}

	template <typename Tfunc>
	static OnReturnCallback
	Make(Tfunc &&_onReturnedValue, std::function<void(Peer *)> onTimeout,
		 uint32_t timeoutMilliseconds, Peer *peer,
		 std::shared_ptr<CommandExecutionQueue> executionQueue = nullptr)
	{
		OnReturnCallback ret;
		OnReturnCallback *self = &ret;

		self->onTimeout = onTimeout;
		self->executionQueue = executionQueue;
		self->timeoutTimePoint =
			std::chrono::steady_clock::now() +
			(std::chrono::milliseconds(timeoutMilliseconds));
		self->peer = peer;
		self->onReturnedValue = [_onReturnedValue](Peer *peer, Flags flags,
												   ByteReader &reader) -> void {
			_onReturnedValue(peer, flags);
		};

		return ret;
	}

public:
	std::function<void(Peer *, Flags, ByteReader &)> onReturnedValue;
	std::function<void(Peer *)> onTimeout;
	std::shared_ptr<CommandExecutionQueue> executionQueue;
	std::chrono::time_point<std::chrono::steady_clock> timeoutTimePoint;
	Peer *peer;
};
} // namespace icon6

#endif
