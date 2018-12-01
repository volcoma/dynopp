#include <dynopp/archives/anyarchive.hpp>
#include <dynopp/binder.hpp>
#include <dynopp/object.hpp>
#include <hpp/string_view.hpp>
#include <suitepp/suitepp/suitepp.hpp>

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
				std::string val2 = obj["key2"];
				std::vector<std::string> val3 = obj["key3"];
				T val4 = obj["key4"];
				val1++;
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
	constexpr int calls = 1000000;
	constexpr int tests = 10;
	constexpr int slots = 100;

	{
		using binder = dyno::binder<dyno::anystream, dyno::anystream, std::string>;
		test_binder<binder>("any binder string", calls, slots, tests);

		using object = dyno::object<dyno::anystream, dyno::anystream, std::string>;
		test_object<object>("any object string", calls, tests);
	}

	{
		using binder = dyno::binder<dyno::anystream, dyno::anystream, std::string, hpp::string_view>;
		test_binder<binder>("any binder string_view", calls, slots, tests);

		using object = dyno::object<dyno::anystream, dyno::anystream, std::string, hpp::string_view>;
		test_object<object>("any object string_view", calls, tests);
	}

	{
		using binder = dyno::binder<dyno::anystream, dyno::anystream, std::string, const char*>;
		test_binder<binder>("any binder const char*", calls, slots, tests);

		// using object = dyno::object<dyno::anystream, dyno::anystream, std::string, const char*>;
		// test_object<object>("any object const char*", calls, tests);
	}

	return 0;
}
