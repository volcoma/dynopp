#include "json.hpp"
#include <dynopp/archives/anyarchive.hpp>
#include <dynopp/binder.hpp>
#include <dynopp/object.hpp>
#include <hpp/string_view.hpp>
#include <suitepp/suitepp.hpp>

#include <hpp/utility.hpp>
#include <iostream>
namespace dyno
{
template <typename Key, typename View>
struct object_rep<nlohmann::json, nlohmann::json, Key, View>
{
	using key_t = Key;
	using view_t = View;

	template <typename T>
	auto get(const view_t& id, T& val) const -> std::tuple<bool, bool>
	{
		try
		{
			T res = impl_.at(key_t(id));
			val = std::move(res);
		}
		catch(const nlohmann::json::out_of_range&)
		{
			return std::make_tuple(false, false);
		}
		catch(...)
		{
			return std::make_tuple(true, false);
		}
		return std::make_tuple(true, true);
	}
	auto get(const view_t& id, object_rep& val) const -> std::tuple<bool, bool>
	{
		return get(id, val.impl_);
	}

	template <typename T>
	auto set(const view_t& id, T&& val) -> std::enable_if_t<!std::is_same<std::decay_t<T>, object_rep>::value>
	{
		impl_[key_t(id)] = std::forward<T>(val);
	}

	template <typename T>
	auto set(const view_t& id, T&& val) -> std::enable_if_t<std::is_same<std::decay_t<T>, object_rep>::value>
	{
		impl_[key_t(id)] = val.impl_;
	}
	bool remove(const view_t& id)
	{
		impl_.erase(Key(id));
		return true;
	}

	bool has(const view_t& id) const
	{
		return impl_.find(key_t(id)) != std::end(impl_);
	}

	bool empty() const
	{
		return impl_.empty();
	}

	auto& get_impl()
	{
		return impl_;
	}
	const auto& get_impl() const
	{
		return impl_;
	}

	nlohmann::json impl_;
};

template <typename Key, typename View>
inline std::ostream& operator<<(std::ostream& o,
								const object_rep<nlohmann::json, nlohmann::json, Key, View>& obj)
{
	return o << obj.get_impl();
}
}

struct Foo
{
};

template <typename T>
void test_object(const std::string& test, int calls, int tests)
{
	// clang-format off
	suitepp::test(test + ", calls=" + std::to_string(calls), [&]()
    {
		auto code = [&]() {
			for(int i = 0; i < calls; ++i)
			{
				T obj;
				obj["key1"] = 1;
				obj["key2"] = "some_string_data";
				obj["key3"] = std::vector<std::string>{"str1", "str2", "str3"};
				T inner = obj;
				obj["key4"] = std::move(inner);

				int val1 = obj["key1"];
                (void)val1;
				std::string val2 = obj["key2"];
                (void)val1;
				std::vector<std::string> val3 = obj["key3"];
                (void)val1;

				T val4 = obj["key4"];

                int val11 = val4["key1"];
                (void)val11;

                std::string val22 = val4["key2"];
                (void)val22;

                std::vector<std::string> val33 = val4["key3"];
                (void)val33;

                std::cout << std::setw(4) << val4 << std::endl;

			}
		};

		EXPECT_NOTHROWS(code()).repeat(tests);
	});
	// clang-format on
}

template <typename T>
void test_binder(const std::string& test, int calls, int slots, int tests)
{
	T binder;
	for(int j = 0; j < slots; ++j)
	{
		binder.connect("plugin_on_system_ready", []() {
			auto a = 0;
			a++;
		});
	}

	binder.bind("plugin_on_system_ready", []() {
		auto a = 0;
		a++;
		return a;
	});

	binder.flush_pending();

	// clang-format off
	suitepp::test(test + " multicast, calls=" + std::to_string(calls) + ", slots=" + std::to_string(slots),
	[&]()
    {
	    auto code = [&]()
        {
            for(int i = 0; i < calls; ++i)
            {
                binder.dispatch("plugin_on_system_ready");

            }
        };

        EXPECT_NOTHROWS(code()).repeat(tests);
	});

	suitepp::test(test + " unicast without return, calls=" + std::to_string(calls), [&]()
    {
		auto code = [&]()
        {
			for(int i = 0; i < calls; ++i)
			{
				binder.call("plugin_on_system_ready");
			}
		};

		EXPECT_NOTHROWS(code()).repeat(tests);
	});

	suitepp::test(test + " unicast with return, calls=" + std::to_string(calls), [&]()
    {

		auto code = [&]()
        {
			for(int i = 0; i < calls; ++i)
			{
				binder.template call<int>("plugin_on_system_ready");
			}
		};

		EXPECT_NOTHROWS(code()).repeat(tests);
	});
	// clang-format on
}

int main()
{
	constexpr int calls = 10;
	constexpr int tests = 10;
	constexpr int slots = 100;

	{
		using binder = dyno::binder<dyno::anystream, dyno::anystream, std::string>;
		test_binder<binder>("any binder string", calls, slots, tests);

		using object_rep = dyno::object_rep<dyno::anystream, dyno::anystream, std::string>;
		using object = dyno::object<object_rep>;
		test_object<object>("any object string", calls, tests);
	}

	{
		using binder = dyno::binder<dyno::anystream, dyno::anystream, std::string, hpp::string_view>;
		test_binder<binder>("any binder string_view", calls, slots, tests);

		using object_rep = dyno::object_rep<dyno::anystream, dyno::anystream, std::string, hpp::string_view>;
		using object = dyno::object<object_rep>;
		test_object<object>("any object string_view", calls, tests);
	}

	{
		using object_rep = dyno::object_rep<nlohmann::json, nlohmann::json, std::string, hpp::string_view>;
		using object = dyno::object<object_rep>;
		test_object<object>("json object", calls, tests);
	}

	return 0;
}
