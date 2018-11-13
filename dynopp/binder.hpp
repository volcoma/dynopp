#ifndef DYNO_BINDER_HPP
#define DYNO_BINDER_HPP

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

template <typename OArchive, typename IArchive, typename Key = std::string, typename View = Key,
		  typename Sentinel = std::weak_ptr<void>>
struct binder
{

public:
	using oarchive_t = OArchive;
	using iarchive_t = IArchive;
	using archive_t = archive<OArchive, IArchive>;
	using key_t = Key;
	using view_t = View;
	using sentinel_t = Sentinel;

	static_assert(std::is_constructible<Key, View>::value, "key type must be constructable from view type");

	template <typename F>
	slot_t connect(const View& id, F&& f, std::uint32_t priority = 0);

	template <typename C, typename F>
	slot_t connect(const View& id, C* const object_ptr, F&& f, std::uint32_t priority = 0);

	template <typename F>
	slot_t connect(const View& id, const Sentinel& sentinel, F&& f, std::uint32_t priority = 0);

	template <typename C, typename F>
	slot_t connect(const View& id, const Sentinel& sentinel, C* const object_ptr, F&& f,
				   std::uint32_t priority = 0);

	void disconnect(const View& id, slot_t slot_id);

	template <typename... Args>
	void dispatch(const View& id, Args&&... args);

	template <typename F>
	void bind(const View& id, F&& f);

	template <typename C, typename F>
	void bind(const View& id, C* const object_ptr, F&& f);

	template <typename F>
	void bind(const View& id, const Sentinel& sentinel, F&& f);

	template <typename C, typename F>
	void bind(const View& id, const Sentinel& sentinel, C* const object_ptr, F&& f);

	bool is_bound(const View& id) const;

	void unbind(const View& id);

	template <typename R = void, typename... Args>
	decltype(auto) call(const View& id, Args&&... args);

	void clear();
	void submit_pending();

private:
	template <typename... Args>
	void dispatch_impl(const View& id, Args&&... args);

	template <typename R, typename... Args, typename std::enable_if_t<!std::is_void<R>::value>* = nullptr>
	R call_impl(const View& id, Args&&... args);

	template <typename R = void, typename... Args,
			  typename std::enable_if_t<std::is_void<R>::value>* = nullptr>
	R call_impl(const View& id, Args&&... args);

	std::atomic<slot_t> id_gen_{0};
	inline slot_t generate_id()
	{
		return ++id_gen_;
	}

	struct unicast_info
	{
		/// Sentinel used for life tracking
		hpp::optional<Sentinel> sentinel;
		/// The function wrapper
		delegate_t<OArchive(IArchive&)> unicast;
	};

	struct multicast_info
	{
		slot_t id = 0;
		/// Priority used for sorting
		std::uint32_t priority = 0;
		/// Sentinel used for life tracking
		hpp::optional<Sentinel> sentinel;
		/// The function wrapper
		delegate_t<void(IArchive&)> multicast;
	};
	struct slots
	{
		std::vector<multicast_info> active;
		std::vector<multicast_info> pending;
	};
	void submit_pending(std::vector<multicast_info>& container,
						std::vector<multicast_info>& container_pending);

	std::map<Key, slots, std::less<>> multicast_list_;

	std::map<Key, unicast_info, std::less<>> unicast_list_;
	IArchive iarchive_ = archive_t::create_iarchive(archive_t::create_oarchive());
};

namespace detail
{

template <typename T>
inline std::string diagnostic(const char* func, const T& val)
{
	std::ostringstream os;
	os << "binder.";
	os << func;
	os << "( \"";
	os << val;
	os << "\" ) : ";
	return os.str();
}

template <class C, typename Ret, typename... Ts>
delegate_t<Ret(Ts...)> bind_this(C* c, Ret (C::*m)(Ts...))
{
	return [=](auto&&... args) { return (c->*m)(std::forward<decltype(args)>(args)...); };
}

template <class C, typename Ret, typename... Ts>
delegate_t<Ret(Ts...)> bind_this(const C* c, Ret (C::*m)(Ts...) const)
{
	return [=](auto&&... args) { return (c->*m)(std::forward<decltype(args)>(args)...); };
}

template <class C, typename Ret, typename... Ts>
delegate_t<Ret(Ts...)> bind_this(C* const c, Ret (C::*m)(Ts...) const)
{
	return [=](auto&&... args) { return (c->*m)(std::forward<decltype(args)>(args)...); };
}

template <typename OArchive, typename IArchive, typename F>
inline delegate_t<OArchive(IArchive&)> package_unicast(F&& f)
{
	using archive_t = archive<OArchive, IArchive>;
	using tuple_args = typename hpp::function_traits<F>::arg_types_decayed;

	return [f = std::forward<F>(f)](IArchive & iarchive)
	{
		tuple_args args;
		hpp::for_each(args, [&iarchive](auto& element) {
			if(!archive_t::unpack(iarchive, element))
			{
				throw std::runtime_error("cannot not unpack the expected arguments");
			}
		});

		auto oarchive = archive_t::create_oarchive();
		if_constexpr(std::is_void<hpp::fn_result_of<F>>::value)
		{
			hpp::apply(f, args);
		}
		else_constexpr
		{
			archive_t::pack(oarchive, hpp::apply(f, args));
		}
		end_if_constexpr;
		return oarchive;
	};
}

template <typename OArchive, typename IArchive, typename C, typename F>
inline delegate_t<OArchive(IArchive&)> package_unicast(C* const object_ptr, F&& f)
{
	return package_unicast<OArchive, IArchive>(bind_this(object_ptr, std::forward<F>(f)));
}

template <typename OArchive, typename IArchive, typename F>
inline delegate_t<void(IArchive&)> package_multicast(F&& f)
{
	using archive_t = archive<OArchive, IArchive>;
	using tuple_args = typename hpp::function_traits<F>::arg_types_decayed;

	return [f = std::forward<F>(f)](IArchive & iarchive)
	{
		tuple_args args;
		hpp::for_each(args, [&iarchive](auto& element) {
			if(!archive_t::unpack(iarchive, element))
			{
				throw std::runtime_error("cannot not unpack the expected arguments");
			}
		});

		hpp::apply(f, args);
	};
}

template <typename OArchive, typename IArchive, typename C, typename F>
inline delegate_t<void(IArchive&)> package_multicast(C* const object_ptr, F&& f)
{
	return package_multicast<OArchive, IArchive>(bind_this(object_ptr, std::forward<F>(f)));
}
}

template <typename OArchive, typename IArchive, typename Key, typename View, typename Sentinel>
template <typename F>
slot_t binder<OArchive, IArchive, Key, View, Sentinel>::connect(const View& id, F&& f, std::uint32_t priority)
{
	static_assert(std::is_void<hpp::fn_result_of<F>>::value,
				  "signals cannot have a return type different from void");

	auto& container = multicast_list_[Key(id)];
	container.pending.emplace_back();
	auto& info = container.pending.back();
	info.priority = priority;
	info.multicast = detail::package_multicast<OArchive, IArchive>(std::forward<F>(f));
	info.id = generate_id();

	return info.id;
}

template <typename OArchive, typename IArchive, typename Key, typename View, typename Sentinel>
template <typename C, typename F>
slot_t binder<OArchive, IArchive, Key, View, Sentinel>::connect(const View& id, C* const object_ptr, F&& f,
																std::uint32_t priority)
{
	static_assert(std::is_void<hpp::fn_result_of<F>>::value,
				  "signals cannot have a return type different from void");

	auto& container = multicast_list_[Key(id)];
	container.pending.emplace_back();
	auto& info = container.pending.back();
	info.priority = priority;
	info.multicast = detail::package_multicast<OArchive, IArchive>(object_ptr, std::forward<F>(f));
	info.id = generate_id();

	return info.id;
}

template <typename OArchive, typename IArchive, typename Key, typename View, typename Sentinel>
template <typename F>
slot_t binder<OArchive, IArchive, Key, View, Sentinel>::connect(const View& id, const Sentinel& sentinel,
																F&& f, std::uint32_t priority)
{
	static_assert(std::is_void<hpp::fn_result_of<F>>::value,
				  "signals cannot have a return type different from void");

	auto& container = multicast_list_[Key(id)];
	container.pending.emplace_back();
	auto& info = container.pending.back();
	info.priority = priority;
	info.sentinel = sentinel;
	info.multicast = detail::package_multicast<OArchive, IArchive>(std::forward<F>(f));
	info.id = generate_id();

	return info.id;
}

template <typename OArchive, typename IArchive, typename Key, typename View, typename Sentinel>
template <typename C, typename F>
slot_t binder<OArchive, IArchive, Key, View, Sentinel>::connect(const View& id, const Sentinel& sentinel,
																C* const object_ptr, F&& f,
																std::uint32_t priority)
{
	static_assert(std::is_void<hpp::fn_result_of<F>>::value,
				  "signals cannot have a return type different from void");

	auto& container = multicast_list_[Key(id)];
	container.pending.emplace_back();
	auto& info = container.pending.back();
	info.priority = priority;
	info.sentinel = sentinel;
	info.multicast = detail::package_multicast<OArchive, IArchive>(object_ptr, std::forward<F>(f));
	info.id = generate_id();

	return info.id;
}

template <typename OArchive, typename IArchive, typename Key, typename View, typename Sentinel>
void binder<OArchive, IArchive, Key, View, Sentinel>::disconnect(const View& id, slot_t slot_id)
{
	auto find_it = multicast_list_.find(id);
	if(find_it != std::end(multicast_list_))
	{
		const auto predicate = [slot_id](const auto& info) { return info.id == slot_id; };
		auto& container_pending = find_it->second.pending;
		auto& container = find_it->second.active;

		// First check the active slots
		auto it = std::find_if(std::begin(container), std::end(container), predicate);
		if(it != std::end(container))
		{
			// just invalidate the sentinel.
			auto& info = *it;
			info.sentinel = {};
		}
		else
		{
			// check and erase from pending
			container_pending.erase(
				std::remove_if(std::begin(container_pending), std::end(container_pending), predicate),
				std::end(container_pending));
		}
	}
}

template <typename OArchive, typename IArchive, typename Key, typename View, typename Sentinel>
template <typename... Args>
void binder<OArchive, IArchive, Key, View, Sentinel>::dispatch(const View& id, Args&&... args)
{
	dispatch_impl(id, std::forward<Args>(args)...);
}

template <typename OArchive, typename IArchive, typename Key, typename View, typename Sentinel>
template <typename... Args>
inline void binder<OArchive, IArchive, Key, View, Sentinel>::dispatch_impl(const View& id, Args&&... args)
{
	auto find_it = multicast_list_.find(id);
	if(find_it == std::end(multicast_list_))
	{
		return;
	}
	auto& container_pending = find_it->second.pending;
	auto& container = find_it->second.active;

	submit_pending(container, container_pending);

	if(container.empty())
	{
		return;
	}
	bool collect_garbage = false;

	// purely for optimizations
	if_constexpr(sizeof...(Args) == 0)
	{
		for(const auto& info : container)
		{
			try
			{
				// check if subscriber expired
				if(info.sentinel)
				{
					// Keep sentinel locked until end of call
					auto sentinel = info.sentinel.value().lock();
					if(!sentinel)
					{
						collect_garbage = true;
						continue;
					}
					else if(lifetime<decltype(sentinel)>::is_paused(sentinel))
					{
						continue;
					}
					else
					{
						info.multicast(iarchive_);
					}
				}
				else
				{
					info.multicast(iarchive_);
				}

				archive_t::rewind(iarchive_);
			}
			catch(const std::exception& e)
			{
				throw std::runtime_error(detail::diagnostic(__func__, id) + e.what());
			}
		}
	}
	else_constexpr
	{
		auto oarchive = archive_t::create_oarchive();
		archive_t::pack(oarchive, std::forward<Args>(args)...);
		auto iarchive = archive_t::create_iarchive(std::move(oarchive));

		for(const auto& info : container)
		{
			try
			{
				// check if subscriber expired
				if(info.sentinel)
				{
					// Keep sentinel locked until end of call
					auto sentinel = info.sentinel.value().lock();
					if(!sentinel)
					{
						collect_garbage = true;
						continue;
					}
					else if(lifetime<decltype(sentinel)>::is_paused(sentinel))
					{
						continue;
					}
					else
					{
						info.multicast(iarchive);
					}
				}
				else
				{
					info.multicast(iarchive);
				}
			}
			catch(const std::exception& e)
			{
				throw std::runtime_error(detail::diagnostic(__func__, id) + e.what());
			}

			archive_t::rewind(iarchive);
		}
	}
	end_if_constexpr;

	if(collect_garbage)
	{
		container.erase(
			std::remove_if(std::begin(container), std::end(container),
						   [](const auto& info) { return info.sentinel && info.sentinel.value().expired(); }),
			std::end(container));
		if(container.empty())
		{
			multicast_list_.erase(find_it);
		}
	}
}
/////////////////

template <typename OArchive, typename IArchive, typename Key, typename View, typename Sentinel>
template <typename F>
void binder<OArchive, IArchive, Key, View, Sentinel>::bind(const View& id, F&& f)
{
	auto& info = unicast_list_[Key(id)];
	info.unicast = detail::package_unicast<OArchive, IArchive>(std::forward<F>(f));
}

template <typename OArchive, typename IArchive, typename Key, typename View, typename Sentinel>
template <typename C, typename F>
void binder<OArchive, IArchive, Key, View, Sentinel>::bind(const View& id, C* const object_ptr, F&& f)
{
	auto& info = unicast_list_[Key(id)];
	info.unicast = detail::package_unicast<OArchive, IArchive>(object_ptr, std::forward<F>(f));
}

template <typename OArchive, typename IArchive, typename Key, typename View, typename Sentinel>
template <typename F>
void binder<OArchive, IArchive, Key, View, Sentinel>::bind(const View& id, const Sentinel& sentinel, F&& f)
{
	auto& info = unicast_list_[Key(id)];
	info.sentinel = sentinel;
	info.unicast = detail::package_unicast<OArchive, IArchive>(std::forward<F>(f));
}

template <typename OArchive, typename IArchive, typename Key, typename View, typename Sentinel>
template <typename C, typename F>
void binder<OArchive, IArchive, Key, View, Sentinel>::bind(const View& id, const Sentinel& sentinel,
														   C* const object_ptr, F&& f)
{
	auto& info = unicast_list_[Key(id)];
	info.sentinel = sentinel;
	info.unicast = detail::package_unicast<OArchive, IArchive>(object_ptr, std::forward<F>(f));
}

template <typename OArchive, typename IArchive, typename Key, typename View, typename Sentinel>
bool binder<OArchive, IArchive, Key, View, Sentinel>::is_bound(const View& id) const
{
	auto it = unicast_list_.find(id);
	return it != std::end(unicast_list_);
}
template <typename OArchive, typename IArchive, typename Key, typename View, typename Sentinel>
void binder<OArchive, IArchive, Key, View, Sentinel>::unbind(const View& id)
{
	auto it = unicast_list_.find(id);
	if(it != std::end(unicast_list_))
	{
		unicast_list_.erase(it);
	}
}

template <typename OArchive, typename IArchive, typename Key, typename View, typename Sentinel>
template <typename R, typename... Args>
decltype(auto) binder<OArchive, IArchive, Key, View, Sentinel>::call(const View& id, Args&&... args)
{
	return call_impl<R>(id, std::forward<Args>(args)...);
}

template <typename OArchive, typename IArchive, typename Key, typename View, typename Sentinel>
template <typename R, typename... Args, typename std::enable_if_t<!std::is_void<R>::value>*>
inline R binder<OArchive, IArchive, Key, View, Sentinel>::call_impl(const View& id, Args&&... args)
{
	static_assert(!std::is_reference<R>::value, "unsupported return by reference (use return by value)");

	auto it = unicast_list_.find(id);
	if(it == std::end(unicast_list_))
	{
		throw std::runtime_error(detail::diagnostic(__func__, id) +
								 "invoking a non-binded function and expecting a return value");
	}
	const auto& info = it->second;

	try
	{
		R res{};
		// Just for optimization purposes
		if_constexpr(sizeof...(Args) == 0)
		{
			// check if subscriber expired
			if(info.sentinel)
			{
				// Keep the sentinel locked until end of call
				auto sentinel = info.sentinel.value().lock();
				if(!sentinel)
				{
					unicast_list_.erase(it);
					throw std::runtime_error(detail::diagnostic(__func__, id) +
											 "invoking a non-binded function and expecting a return value");
				}
				else
				{
					auto result_oarchive = info.unicast(iarchive_);

					// rewind the internal archive when using it
					archive_t::rewind(iarchive_);

					auto result_iarchive = archive_t::create_iarchive(std::move(result_oarchive));
					if(!archive_t::unpack(result_iarchive, res))
					{
						throw std::runtime_error("cannot unpack the expected return type");
					}
				}
			}
			else
			{
				auto result_oarchive = info.unicast(iarchive_);

				// rewind the internal archive when using it
				archive_t::rewind(iarchive_);

				auto result_iarchive = archive_t::create_iarchive(std::move(result_oarchive));
				if(!archive_t::unpack(result_iarchive, res))
				{
					throw std::runtime_error("cannot unpack the expected return type");
				}
			}
		}
		else_constexpr
		{

			auto oarchive = archive_t::create_oarchive();
			archive_t::pack(oarchive, std::forward<Args>(args)...);
			auto iarchive = archive_t::create_iarchive(std::move(oarchive));

			// check if subscriber expired
			if(info.sentinel)
			{
				// Keep the sentinel locked until end of call
				auto sentinel = info.sentinel.value().lock();
				if(!sentinel)
				{
					unicast_list_.erase(it);
					throw std::runtime_error(detail::diagnostic(__func__, id) +
											 "invoking a non-binded function and expecting a return value");
				}
				else
				{
					auto result_oarchive = info.unicast(iarchive);
					auto result_iarchive = archive_t::create_iarchive(std::move(result_oarchive));
					if(!archive_t::unpack(result_iarchive, res))
					{
						throw std::runtime_error("cannot unpack the expected return type");
					}
				}
			}
			else
			{
				auto result_oarchive = info.unicast(iarchive);
				auto result_iarchive = archive_t::create_iarchive(std::move(result_oarchive));
				if(!archive_t::unpack(result_iarchive, res))
				{
					throw std::runtime_error("cannot unpack the expected return type");
				}
			}
		}
		end_if_constexpr;

		return res;
	}
	catch(const std::exception& e)
	{
		throw std::runtime_error(detail::diagnostic(__func__, id) + e.what());
	}
}

template <typename OArchive, typename IArchive, typename Key, typename View, typename Sentinel>
template <typename R, typename... Args, typename std::enable_if_t<std::is_void<R>::value>*>
inline R binder<OArchive, IArchive, Key, View, Sentinel>::call_impl(const View& id, Args&&... args)
{
	auto it = unicast_list_.find(id);

	if(it == std::end(unicast_list_))
	{
		throw std::runtime_error(detail::diagnostic(__func__, id) + "invoking a non-binded function");
	}

	const auto& info = it->second;
	try
	{
		// Just for optimization purposes
		if_constexpr(sizeof...(Args) == 0)
		{
			// check if subscriber expired
			if(info.sentinel)
			{
				// Keep sentinel locked until end of call
				auto sentinel = info.sentinel.value().lock();
				if(!sentinel)
				{
					unicast_list_.erase(it);
					throw std::runtime_error(detail::diagnostic(__func__, id) +
											 "invoking a non-binded function");
				}

				info.unicast(iarchive_);
			}
			else
			{
				info.unicast(iarchive_);
			}
			// rewind the internal archive when using it
			archive_t::rewind(iarchive_);
		}
		else_constexpr
		{
			auto oarchive = archive_t::create_oarchive();
			archive_t::pack(oarchive, std::forward<Args>(args)...);
			auto iarchive = archive_t::create_iarchive(std::move(oarchive));

			// check if subscriber expired
			if(info.sentinel)
			{
				// Keep sentinel locked until end of call
				auto sentinel = info.sentinel.value().lock();
				if(!sentinel)
				{
					unicast_list_.erase(it);
					throw std::runtime_error(detail::diagnostic(__func__, id) +
											 "invoking a non-binded function");
				}

				info.unicast(iarchive);
			}
			else
			{
				info.unicast(iarchive);
			}
		}
		end_if_constexpr;
	}
	catch(const std::exception& e)
	{
		throw std::runtime_error(detail::diagnostic(__func__, id) + e.what());
	}
}

template <typename OArchive, typename IArchive, typename Key, typename View, typename Sentinel>
void binder<OArchive, IArchive, Key, View, Sentinel>::clear()
{
	multicast_list_.clear();
	unicast_list_.clear();
}

template <typename OArchive, typename IArchive, typename Key, typename View, typename Sentinel>
void binder<OArchive, IArchive, Key, View, Sentinel>::submit_pending()
{
	for(auto& kvp : multicast_list_)
	{
		auto& container_pending = kvp.second.pending;
		auto& container = kvp.second.active;

		submit_pending(container, container_pending);
	}
}

template <typename OArchive, typename IArchive, typename Key, typename View, typename Sentinel>
inline void binder<OArchive, IArchive, Key, View, Sentinel>::submit_pending(
	std::vector<multicast_info>& container, std::vector<multicast_info>& container_pending)
{
	if(!container_pending.empty())
	{
		std::move(std::begin(container_pending), std::end(container_pending), std::back_inserter(container));
		container_pending.clear();

		std::sort(std::begin(container), std::end(container),
				  [](const auto& lhs, const auto& rhs) { return lhs.priority > rhs.priority; });
	}
}
}
#endif