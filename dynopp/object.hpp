#ifndef DYNO_OBJECT_HPP
#define DYNO_OBJECT_HPP

#include <algorithm>
#include <atomic>
#include <cassert>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "archive.h"
#include <hpp/optional.hpp>
#include <hpp/type_traits.hpp>
#include <hpp/utility.hpp>

namespace dyno
{

template <typename OArchive, typename IArchive, typename Key, typename View>
struct proxy_op;

template <typename OArchive, typename IArchive, typename Key = std::string, typename View = Key>
struct object
{
	static_assert(std::is_constructible<Key, View>::value, "key type must be constructable from view type");

public:
	using archive_t = archive<OArchive, IArchive>;
	using proxy_op_t = proxy_op<OArchive, IArchive, Key, View>;
	using container_t = std::map<Key, typename archive_t::storage_t, std::less<>>;
	friend struct proxy_op<OArchive, IArchive, Key, View>;

	//-----------------------------------------------------------------------------
	/// Sets a value to the field with name 'id'
	//-----------------------------------------------------------------------------
	template <typename T>
	void set(const View& id, T&& val);

	//-----------------------------------------------------------------------------
	/// Removes a field with name 'id'
	//-----------------------------------------------------------------------------
	void set(const View& id, std::nullptr_t);

	//-----------------------------------------------------------------------------
	/// Tries to retrive a field
	//-----------------------------------------------------------------------------
	template <typename T>
	bool get(const View& id, T& val) const;

	//-----------------------------------------------------------------------------
	/// Checks whether a field exists
	//-----------------------------------------------------------------------------
	bool has(const View& id) const;

	//-----------------------------------------------------------------------------
	/// Tries to retrive a field
	//-----------------------------------------------------------------------------
	auto operator[](const View& id) -> proxy_op_t;

	//-----------------------------------------------------------------------------
	/// Checks if the object is empty
	//-----------------------------------------------------------------------------
	bool empty() const;

	//-----------------------------------------------------------------------------
	/// Retrieves the internal values
	//-----------------------------------------------------------------------------
	auto get_values() -> container_t&;
	auto get_values() const -> const container_t&;

private:
	template <typename T>
	std::tuple<bool, bool> get_verbose(const View& id, T& val) const;
	bool erase(const View& id);

	container_t values_;
};

template <typename OArchive, typename IArchive, typename Key, typename View>
template <typename T>
void object<OArchive, IArchive, Key, View>::set(const View& id, T&& val)
{
	auto oarchive = archive_t::create_oarchive();
	archive_t::pack(oarchive, std::forward<T>(val));
	auto find_it = values_.find(id);
	if(find_it != std::end(values_))
	{
		find_it->second = archive_t::get_storage(std::move(oarchive));
	}
	else
	{
		values_.emplace(Key(id), archive_t::get_storage(std::move(oarchive)));
	}
}

template <typename OArchive, typename IArchive, typename Key, typename View>
void object<OArchive, IArchive, Key, View>::set(const View& id, std::nullptr_t)
{
	erase(id);
}

template <typename OArchive, typename IArchive, typename Key, typename View>
template <typename T>
bool object<OArchive, IArchive, Key, View>::get(const View& id, T& val) const
{
	bool exists{};
	bool unpacked{};
	std::tie(exists, unpacked) = get_verbose(id, val);
	return exists && unpacked;
}

template <typename OArchive, typename IArchive, typename Key, typename View>
bool object<OArchive, IArchive, Key, View>::has(const View& id) const
{
	auto find_it = values_.find(id);
	return find_it != std::end(values_);
}

template <typename OArchive, typename IArchive, typename Key, typename View>
auto object<OArchive, IArchive, Key, View>::operator[](const View& id) -> proxy_op_t
{
	return {id, *this};
}

template <typename OArchive, typename IArchive, typename Key, typename View>
bool object<OArchive, IArchive, Key, View>::empty() const
{
	return values_.empty();
}

template <typename OArchive, typename IArchive, typename Key, typename View>
auto object<OArchive, IArchive, Key, View>::get_values() -> container_t&
{
	return values_;
}

template <typename OArchive, typename IArchive, typename Key, typename View>
auto object<OArchive, IArchive, Key, View>::get_values() const -> const container_t&
{
	return values_;
}

template <typename OArchive, typename IArchive, typename Key, typename View>
template <typename T>
std::tuple<bool, bool> object<OArchive, IArchive, Key, View>::get_verbose(const View& id, T& val) const
{
	using result_type = std::tuple<bool, bool>;
	auto it = values_.find(id);
	if(it == std::end(values_))
	{
		return result_type{false, false};
	}
	const auto& storage = it->second;
	auto iarchive = archive_t::create_iarchive(storage);
	bool unpacked = archive_t::unpack(iarchive, val);
	return result_type{true, unpacked};
}

template <typename OArchive, typename IArchive, typename Key, typename View>
bool object<OArchive, IArchive, Key, View>::erase(const View& id)
{
	auto it = values_.find(id);
	if(it == std::end(values_))
	{
		return false;
	}

	values_.erase(it);
	return true;
}

template <typename OArchive, typename IArchive, typename Key, typename View>
struct proxy_op
{
	using object_t = object<OArchive, IArchive, Key, View>;

public:
	proxy_op(const View& id, object_t& obj)
		: key_(id)
		, obj_(obj)
	{
	}
	proxy_op(const proxy_op&) = delete;
	proxy_op(proxy_op&&) = delete;
	proxy_op& operator=(const proxy_op&) = delete;
	proxy_op& operator=(proxy_op&&) = delete;

	template <typename T>
	operator T() const
	{
		T val{};
		bool exists{};
		bool unpacked{};
		std::tie(exists, unpacked) = obj_.get_verbose(key_, val);
		if(exists)
		{
			if(unpacked)
			{
				return val;
			}
			throw std::invalid_argument("\"" + key_ + "\" - could not unpack to the expected type");
		}

		throw std::out_of_range("\"" + key_ + "\" - no such field exists");
	}

	template <typename T>
	operator hpp::optional<T>() const
	{
		hpp::optional<T> val{};
		obj_.get(key_, *val);
		return val;
	}

	template <typename T>
	proxy_op& operator=(T&& val)
	{
		obj_.set(key_, std::forward<T>(val));
		return *this;
	}

	template <typename T>
	T value_or(T&& default_val) const
	{
		T val{};
		if(obj_.get(key_, val))
		{
			return val;
		}
		return std::forward<T>(default_val);
	}

private:
	Key key_;
	object_t& obj_;
};
}
#endif
