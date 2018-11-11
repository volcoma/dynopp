#pragma once
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace dyno
{

template <typename OArchive, typename IArchive>
struct archive
{
	static_assert(sizeof(OArchive) == 0 || sizeof(IArchive) == 0, "Specialization required!");

	static OArchive create_oarchive();
	static IArchive create_iarchive(OArchive&&);

	static void rewind(IArchive&);

	template <typename... Args>
	static bool pack(OArchive&, Args&&...);

	template <typename T>
	static bool unpack(IArchive&, T&);
};

template <typename Sentinel>
struct lifetime
{
	static_assert(sizeof(Sentinel) == 0, "Specialization required!");

	static bool is_paused(const Sentinel&);
};

template <>
struct lifetime<std::shared_ptr<void>>
{
	static bool is_paused(const std::shared_ptr<void>&)
	{
		return false;
	}
};

using slot_t = std::uint64_t;

template <typename T>
using delegate_t = std::function<T>;
}
