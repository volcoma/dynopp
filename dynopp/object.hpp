#ifndef DYNO_OBJECT_HPP
#define DYNO_OBJECT_HPP

#include <algorithm>
#include <atomic>
#include <cassert>
#include <functional>
#include <iomanip>
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

template <typename OArchive, typename IArchive, typename Key = std::string, typename View = Key>
struct object_rep
{
	using archive_t = archive<OArchive, IArchive>;
	using impl_t = std::map<Key, typename archive_t::storage_t, std::less<>>;
	using key_t = Key;
	using view_t = View;

	template <typename T>
	auto get(const View& id, T& val) const -> std::tuple<bool, bool>;

	template <typename T>
	void set(const View& id, T&& val);

	bool remove(const View& id);

	bool has(const View& id) const;

	bool empty() const;

	auto& get_impl();
	const auto& get_impl() const;

	impl_t impl_;
};

template <typename Rep>
struct proxy_op;

template <typename Rep>
struct object
{

public:
	using rep_t = Rep;
	using key_t = typename Rep::key_t;
	using view_t = typename Rep::view_t;
	static_assert(std::is_constructible<key_t, view_t>::value,
				  "key type must be constructable from view type");
	static_assert(std::is_constructible<view_t, key_t>::value,
				  "view type must be constructable from key type");
	using proxy_op_t = proxy_op<Rep>;
	friend struct proxy_op<Rep>;

	//-----------------------------------------------------------------------------
	/// Sets a value to the field with name 'id'
	//-----------------------------------------------------------------------------
	template <typename T>
	void set(const view_t& id, T&& val);

	//-----------------------------------------------------------------------------
	/// Removes a field with name 'id'
	//-----------------------------------------------------------------------------
	void set(const view_t& id, std::nullptr_t);

	//-----------------------------------------------------------------------------
	/// Tries to retrive a field
	//-----------------------------------------------------------------------------
	template <typename T>
	bool get(const view_t& id, T& val) const;

	//-----------------------------------------------------------------------------
	/// Checks whether a field exists
	//-----------------------------------------------------------------------------
	bool has(const view_t& id) const;

	//-----------------------------------------------------------------------------
	/// Tries to retrive a field
	//-----------------------------------------------------------------------------
	auto operator[](const view_t& id) -> proxy_op_t;

	//-----------------------------------------------------------------------------
	/// Checks if the object is empty
	//-----------------------------------------------------------------------------
	bool empty() const;

	//-----------------------------------------------------------------------------
	/// Retrieves the internal values
	//-----------------------------------------------------------------------------
	auto get_rep() -> rep_t&;
	auto get_rep() const -> const rep_t&;

private:
	template <typename T>
	auto rep_get(const view_t& id, T& val) const
		-> std::enable_if_t<false == std::is_same<std::decay_t<T>, object>::value, std::tuple<bool, bool>>
	{
		return rep_.get(id, val);
	}
	template <typename T>
	auto rep_get(const view_t& id, T& val) const
		-> std::enable_if_t<true == std::is_same<std::decay_t<T>, object>::value, std::tuple<bool, bool>>
	{
		return rep_get(id, val.rep_);
	}

	template <typename T>
	auto rep_set(const view_t& id, T&& val)
		-> std::enable_if_t<false == std::is_same<std::decay_t<T>, object>::value>
	{
		return rep_.set(id, std::forward<T>(val));
	}
	template <typename T>
	auto rep_set(const view_t& id, T&& val)
		-> std::enable_if_t<true == std::is_same<std::decay_t<T>, object>::value>
	{
		return rep_set(id, val.rep_);
	}

	bool rep_remove(const view_t& id);

	bool rep_has(const view_t& id) const;

	rep_t rep_;
};

//-------------------------------------------------
// IMPL
//-------------------------------------------------
template <typename OArchive, typename IArchive, typename Key, typename View>
inline std::ostream& operator<<(std::ostream& o, const object_rep<OArchive, IArchive, Key, View>& obj)
{
	const auto w = size_t(o.width());

	o.write("{\n", 2);
	std::string indent_string{};
	if(indent_string.size() < w)
	{
		indent_string.resize(w, ' ');
	}
	for(const auto& kvp : obj.get_impl())
	{
		o << indent_string << to_string(kvp.first) << ": " << to_string(kvp.second) << ",\n";
	}
	o.write("}\n", 2);
	return o;
}

template <typename OArchive, typename IArchive, typename Key, typename View>
bool object_rep<OArchive, IArchive, Key, View>::empty() const
{
	return impl_.empty();
}

template <typename OArchive, typename IArchive, typename Key, typename View>
bool object_rep<OArchive, IArchive, Key, View>::has(const View& id) const
{
	return impl_.find(id) != std::end(impl_);
}

template <typename OArchive, typename IArchive, typename Key, typename View>
template <typename T>
void object_rep<OArchive, IArchive, Key, View>::set(const View& id, T&& val)
{
	auto oarchive = archive_t::create_oarchive();
	archive_t::pack(oarchive, std::forward<T>(val));
	auto find_it = impl_.find(id);
	if(find_it != std::end(impl_))
	{
		find_it->second = archive_t::get_storage(std::move(oarchive));
	}
	else
	{
		impl_.emplace(Key(id), archive_t::get_storage(std::move(oarchive)));
	}
}

template <typename OArchive, typename IArchive, typename Key, typename View>
template <typename T>
auto object_rep<OArchive, IArchive, Key, View>::get(const View& id, T& val) const -> std::tuple<bool, bool>
{
	using result_type = std::tuple<bool, bool>;
	auto it = impl_.find(id);
	if(it == std::end(impl_))
	{
		return result_type{false, false};
	}
	const auto& storage = it->second;
	auto iarchive = archive_t::create_iarchive(storage);
	bool unpacked = archive_t::unpack(iarchive, val);
	return result_type{true, unpacked};
}

template <typename OArchive, typename IArchive, typename Key, typename View>
bool object_rep<OArchive, IArchive, Key, View>::remove(const View& id)
{
	auto it = impl_.find(id);
	if(it == std::end(impl_))
	{
		return false;
	}

	impl_.erase(it);
	return true;
}
template <typename OArchive, typename IArchive, typename Key, typename View>
auto& object_rep<OArchive, IArchive, Key, View>::get_impl()
{
	return impl_;
}

template <typename OArchive, typename IArchive, typename Key, typename View>
const auto& object_rep<OArchive, IArchive, Key, View>::get_impl() const
{
	return impl_;
}

template <typename Rep>
template <typename T>
void object<Rep>::set(const view_t& id, T&& val)
{
	rep_set(id, std::forward<T>(val));
}

template <typename Rep>
void object<Rep>::set(const view_t& id, std::nullptr_t)
{
	rep_remove(id);
}

template <typename Rep>
template <typename T>
bool object<Rep>::get(const view_t& id, T& val) const
{
	bool exists{};
	bool unpacked{};
	std::tie(exists, unpacked) = rep_get(id, val);
	return exists && unpacked;
}

template <typename Rep>
bool object<Rep>::has(const view_t& id) const
{
	return rep_has(id);
}

template <typename Rep>
auto object<Rep>::operator[](const view_t& id) -> proxy_op_t
{
	return {id, *this};
}

template <typename Rep>
bool object<Rep>::empty() const
{
	return rep_.empty();
}

template <typename Rep>
auto object<Rep>::get_rep() -> rep_t&
{
	return rep_;
}

template <typename Rep>
auto object<Rep>::get_rep() const -> const rep_t&
{
	return rep_;
}

template <typename Rep>
bool object<Rep>::rep_has(const view_t& id) const
{
	return rep_.has(id);
}

template <typename Rep>
bool object<Rep>::rep_remove(const view_t& id)
{
	return rep_.remove(id);
}

template <typename Rep>
inline std::ostream& operator<<(std::ostream& o, const object<Rep>& obj)
{
	return o << obj.get_rep();
}

template <typename Rep>
struct proxy_op
{
	using object_t = object<Rep>;
	using key_t = typename Rep::key_t;
	using view_t = typename Rep::view_t;

public:
	proxy_op(const view_t& id, object_t& obj)
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
		std::tie(exists, unpacked) = obj_.template rep_get<T>(key_, val);
		if(exists)
		{
			if(unpacked)
			{
				return val;
			}
			throw std::invalid_argument(to_string(key_) + " - could not unpack to the expected type");
		}

		throw std::out_of_range(to_string(key_) + " - no such field exists");
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
	key_t key_;
	object_t& obj_;
};
}
#endif
