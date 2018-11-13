#pragma once
#include "../archive.h"
#include "anystream.hpp"
#include <hpp/utility.hpp>

namespace dyno
{
template <>
struct archive<anystream, anystream>
{
	using oarchive_t = anystream;
	using iarchive_t = anystream;

	static oarchive_t create_oarchive()
	{
		return {};
	}
	static iarchive_t create_iarchive(oarchive_t&& oarchive)
	{
		return std::move(oarchive);
	}

	template <typename... Args>
	static void pack(oarchive_t& oarchive, Args&&... args)
	{
		oarchive.storage.reserve(sizeof...(Args));

		hpp::for_each(std::forward_as_tuple(std::forward<Args>(args)...),
					  [&oarchive](auto&& arg) { oarchive << std::forward<decltype(arg)>(arg); });
	}

	template <typename T>
	static bool unpack(iarchive_t& iarchive, T& obj)
	{
		iarchive >> obj;
		return static_cast<bool>(iarchive);
	}

	static void rewind(iarchive_t& iarchive)
	{
		iarchive.rewind();
	}
};
}
