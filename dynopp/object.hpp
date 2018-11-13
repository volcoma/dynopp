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
	friend struct proxy_op<OArchive, IArchive, Key, View>;

	template <typename T>
	void set(const View& id, T&& val)
	{
		auto oarchive = archive_t::create_oarchive();
		archive_t::pack(oarchive, std::forward<T>(val));
		auto find_it = values_.find(id);
		if(find_it != std::end(values_))
		{
			find_it->second = archive_t::create_iarchive(std::move(oarchive));
		}
		else
		{
			values_[Key(id)] = archive_t::create_iarchive(std::move(oarchive));
		}
	}

	void set(const View& id, std::nullptr_t)
	{
		erase(id);
	}

	template <typename T>
	bool get(const View& id, T& val)
	{
		bool exists{};
		bool unpacked{};
		std::tie(exists, unpacked) = get_verbose(id, val);
		return exists;
	}

	bool has(const View& id)
	{
		auto find_it = values_.find(id);
		return find_it != std::end(values_);
	}

	proxy_op_t operator[](const View& id)
	{
		return {id, *this};
	}

	auto& get_values()
	{
		return values_;
	}
	const auto& get_values() const
	{
		return values_;
	}

private:
	template <typename T>
	std::tuple<bool, bool> get_verbose(const View& id, T& val)
	{
		using result_type = std::tuple<bool, bool>;
		auto it = values_.find(id);
		if(it == std::end(values_))
		{
			return result_type{false, false};
		}
		auto& iarchive = it->second;
		bool unpacked = archive_t::unpack(iarchive, val);
		archive_t::rewind(iarchive);
		return result_type{true, unpacked};
	}

	bool erase(const View& id)
	{
		auto it = values_.find(id);
		if(it == std::end(values_))
		{
			return false;
		}

		values_.erase(it);
		return true;
	}
	std::map<Key, IArchive, std::less<>> values_;
};

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
	T value_or(T&& default_val)
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
