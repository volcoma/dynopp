### dynopp c++14 library for dynamic dispatch and dynamic objects 

```c++

// Here we are making an alias of a binder with a key type = std::string
using anybinder = dyno::binder<dyno::anystream, dyno::anystream, std::string>;
//you can also provide a view_type for some more optimizaiton
//using anybinder = dyno::binder<dyno::anystream, dyno::anystream, std::string, std::string_view>;
    
anybinder binder;
    
binder.connect("on_some_event", []()
{
    // logic
});
    
binder.connect("on_some_event", [](int arg1, const std::string& arg2)
{
    // logic
});
    
// this will call all the slots. If someone expects more arguments than
// the ones provied an exception will be thrown. Also if
// the types are missmatched an exception will be thrown.
// Type matching is provided by the underlying serializer/deserializer
binder.dispatch("on_some_event", 12, "wooow");
   
binder.bind("call_with_return", [](const std::string& arg1, float arg2)
{
    return 42;
});
    
int result = binder.call<int>("call_with_return", "somearg1", 12.0f);
    
// remove binded function
binder.unbind("call_with_return");
    
// in order to disconnect a slot you need to keep the
// slot_id returned when connecting.
auto slot_id = binder.connect("on_some_event", []()
{
});
    
// disconnect a slot    
binder.disconnect("on_some_event", slot_id);


// You can also create a dynamic object type
// It will behave more or less like a fully dynamic type
using anyobject = dyno::object<dyno::anystream, dyno::anystream, std::string>;
//you can also provide a view_type for some more optimizaiton
//using anyobject = dyno::object<dyno::anystream, dyno::anystream, std::string, std::string_view>;
anyobject obj;
obj["key1"] = 1;
obj["key2"] = "some_string_data";
obj["key3"] = std::vector<std::string>{"str1", "str2", "str3"};
//you can copy it
anyobject inner = obj;
//or nest them inside eachother
obj["key4"] = std::move(inner);

// retrieve fields like so:
int val1 = obj["key1"];
std::string val2 = obj["key2"];
std::vector<std::string> val3 = obj["key3"];   
anyobject val4 = obj["key4"];
    
// this will throw if the key is not present or the value' type cannot be matched
// again type matching is provided by the user. Look below.
int val5 = obj["key5"];
    
// you can provide a default value if the key doesn't exist.
int val6 = obj["key6"].value_or(4);
// you can also explicitly check
if(obj.has("key6"))
{
}

// or try to get it via an out param
int val{};
if(obj.get("key6", val))
{
}
```

For this all to work out you need to provide a serialize/deserialize customization point
with archives that you are working with. 
The example above works with an anystream which is basically a vector<std::any>, but can work
with any other binary stream if you provide a specialization
```c++
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
```

### Performance
Keep in mind that this is a purely dynamic dispatch and serialization/deserialization is involved.
If you provide a fast-enough serializer/deserializer you can even speed this up. The library provides
an 'anystream' as shown in the examples, which is basically a vector<std::any>, with any itself having a small size optimization
makes life better.
