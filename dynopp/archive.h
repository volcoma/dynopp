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
	using oarchive_t = OArchive;
	using iarchive_t = IArchive;
	using storage_t = typename oarchive_t::storage_t;

	static_assert(sizeof(oarchive_t) == 0 || sizeof(iarchive_t) == 0, "Specialization required!");

	static oarchive_t create_oarchive();
	static iarchive_t create_iarchive(oarchive_t&&);
	static iarchive_t create_iarchive(const storage_t& storage);
	static storage_t get_storage(oarchive_t&& oarchive);

	static void rewind(iarchive_t&);

	template <typename... Args>
	static bool pack(oarchive_t&, Args&&...);

	template <typename T>
	static bool unpack(iarchive_t&, T&);
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
