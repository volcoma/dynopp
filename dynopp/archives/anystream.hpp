#pragma once
#include <cstddef>
#include <cstdint>
#include <hpp/any.hpp>
#include <vector>

namespace dyno
{
// Non-convertible fallback
template <typename From, typename To,
		  typename std::enable_if<!std::is_convertible<From, To>::value>::type* = nullptr>
bool implicit_cast_impl(const hpp::any& /*operand*/, To& /*result*/)
{
	return false;
}

// Convertible implicit cast
template <typename From, typename To,
		  typename std::enable_if<std::is_convertible<From, To>::value>::type* = nullptr>
bool implicit_cast_impl(const hpp::any& operand, To& result)
{
	auto val = hpp::any_cast<From>(&operand);
	if(val)
	{
		result = static_cast<To>(*val);
		return true;
	}

	return false;
}

// Try to implicit cast an 'any' parameter to a type T
template <typename To>
bool try_implicit_cast(const hpp::any& operand, To& result)
{
	if(implicit_cast_impl<To>(operand, result))
	{
		return true;
	}
	else if(implicit_cast_impl<const char*>(operand, result))
	{
		return true;
	}
	else if(implicit_cast_impl<std::int8_t>(operand, result))
	{
		return true;
	}
	else if(implicit_cast_impl<std::int16_t>(operand, result))
	{
		return true;
	}
	else if(implicit_cast_impl<std::int32_t>(operand, result))
	{
		return true;
	}
	else if(implicit_cast_impl<std::int64_t>(operand, result))
	{
		return true;
	}
	else if(implicit_cast_impl<std::uint8_t>(operand, result))
	{
		return true;
	}
	else if(implicit_cast_impl<std::uint16_t>(operand, result))
	{
		return true;
	}
	else if(implicit_cast_impl<std::uint32_t>(operand, result))
	{
		return true;
	}
	else if(implicit_cast_impl<std::uint64_t>(operand, result))
	{
		return true;
	}
	else if(implicit_cast_impl<float>(operand, result))
	{
		return true;
	}
	else if(implicit_cast_impl<double>(operand, result))
	{
		return true;
	}
	else if(implicit_cast_impl<char>(operand, result))
	{
		return true;
	}
	else if(implicit_cast_impl<unsigned char>(operand, result))
	{
		return true;
	}
	else if(implicit_cast_impl<std::nullptr_t>(operand, result))
	{
		return true;
	}

	return false;
}

struct anystream
{
	using storage_t = std::vector<hpp::any>;
	anystream() = default;
	anystream(const anystream& rhs)
		: internal_storage(*rhs.work_storage)
		, work_storage(std::addressof(internal_storage))
	{
	}
	anystream(anystream&& rhs) noexcept
		: internal_storage(std::move(rhs.internal_storage))
		, work_storage(std::addressof(internal_storage))
	{
	}

	anystream& operator=(const anystream& rhs)
	{
		internal_storage = (*rhs.work_storage);
		work_storage = std::addressof(internal_storage);

		return *this;
	}
	anystream& operator=(anystream&& rhs) noexcept
	{
		internal_storage = std::move(rhs.internal_storage);
		work_storage = std::addressof(internal_storage);

		return *this;
	}
	// create from external storage
	anystream(const storage_t& s)
		: work_storage(&s)
	{
	}

	template <typename T>
	anystream& operator<<(T&& val) noexcept
	{
		const_cast<storage_t*>(work_storage)->emplace_back(std::forward<T>(val));
		return *this;
	}

	template <typename T>
	anystream& operator>>(T& val) noexcept
	{
		if(!is_ok)
		{
			return *this;
		}
		if(idx < work_storage->size())
		{
			const auto& any_obj = (*work_storage)[idx++];
			is_ok &= try_implicit_cast(any_obj, val);
		}
		else
		{
			is_ok = false;
		}

		return *this;
	}

	void rewind() noexcept
	{
		is_ok = true;
		idx = 0;
	}

	explicit operator bool() const noexcept
	{
		return is_ok;
	}

    bool has_external_storage() const
    {
        return work_storage != std::addressof(internal_storage);
    }

	std::size_t idx = 0;
	bool is_ok = true;
	storage_t internal_storage;
	const storage_t* work_storage{std::addressof(internal_storage)};
};
}
