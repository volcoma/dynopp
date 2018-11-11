#include "anystream.hpp"
#include <dynopp/binder.hpp>
#include <dynopp/object.hpp>
#include <hpp/string_view.hpp>
#include <suitepp/suitepp/suitepp.hpp>
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

template <typename T>
void test_object(const std::string& test, int calls, int tests)
{

	suitepp::test(test + ", calls=" + std::to_string(calls), [&]() {

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

	binder.submit_pending();

	suitepp::test(test + " multicast, calls=" + std::to_string(calls) + ", slots=" + std::to_string(slots),
				  [&]() {

					  auto code = [&]() {
						  for(int i = 0; i < calls; ++i)
						  {
							  binder.dispatch("plugin_on_system_ready");
						  }

						  return true;
					  };

					  EXPECT(code()).repeat(tests);

				  });

	suitepp::test(test + " unicast without return, calls=" + std::to_string(calls), [&]() {

		auto code = [&]() {
			for(int i = 0; i < calls; ++i)
			{
				binder.call("plugin_on_system_ready");
			}

			return true;
		};

		EXPECT(code()).repeat(tests);
	});

	suitepp::test(test + " unicast with return, calls=" + std::to_string(calls), [&]() {

		auto code = [&]() {
			for(int i = 0; i < calls; ++i)
			{
				binder.template call<int>("plugin_on_system_ready");
			}

			return true;
		};

		EXPECT(code()).repeat(tests);
	});
}

int main()
{
	using anybinder = dyno::binder<dyno::anystream, dyno::anystream, std::string, hpp::string_view>;
	using anyobject = dyno::object<dyno::anystream, dyno::anystream, std::string, hpp::string_view>;

	constexpr int calls = 1000000;
	constexpr int tests = 10;
	constexpr int slots = 100;

	test_binder<anybinder>("any binder", calls, slots, tests);
	test_object<anyobject>("any object", calls, tests);

	//	{
	//		std::unordered_map<std::string, std::vector<std::function<void()>>> mm;
	//		for(int j = 0; j < slots; ++j)
	//		{
	//			mm["test"].emplace_back([]() {
	//				auto a = 0;
	//				a++;
	//			});
	//		}
	//		suitepp::test("test raw", [&]() {

	//			auto code = [&]() {
	//				for(int i = 0; i < calls; ++i)
	//				{
	//					const auto& cont = mm["test"];
	//					for(const auto& callback : cont)
	//					{
	//						callback();
	//					}
	//				}

	//				return true;
	//			};
	//			EXPECT(code()).repeat(tests);
	//		});
	//	}

	return 0;
}
