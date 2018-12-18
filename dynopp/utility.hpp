#pragma once
#include <hpp/type_traits.hpp>
#include <hpp/utility.hpp>
#include <sstream>
#include <string>

namespace dyno
{

// 1 - detecting if std::to_string is valid on T
template <typename T>
using std_to_string_expression = decltype(std::to_string(std::declval<T>()));
template <typename T>
using has_std_to_string = hpp::is_detected<std_to_string_expression, T>;

// 2 - detecting if to_string is valid on T
template <typename T>
using to_string_expression = decltype(to_string(std::declval<const T>()));
template <typename T>
using has_to_string = hpp::is_detected<to_string_expression, T>;

// 3 - detecting if T can be sent to an ostringstream
template <typename T>
using ostringstream_expression = decltype(std::declval<std::ostringstream&>() << std::declval<T>());
template <typename T>
using has_ostringstream = hpp::is_detected<ostringstream_expression, T>;

// 1 - std::to_string is valid on T
template <typename T, typename std::enable_if<has_std_to_string<T>::value, int>::type = 0>
std::string make_string(const T& t)
{
    return std::to_string(t);
}

// 2 - std::to_string is not valid on T, but to_string is
template <typename T,
          typename std::enable_if<!has_std_to_string<T>::value && has_to_string<T>::value, int>::type = 0>
std::string make_string(const T& t)
{
    return to_string(t);
}

// 3 - neither std::string nor to_string work on T, let's stream it then
template <typename T, typename std::enable_if<!has_std_to_string<T>::value && !has_to_string<T>::value &&
                                                  has_ostringstream<T>::value,
                                              int>::type = 0>
std::string make_string(const T& t)
{
    std::ostringstream oss;
    oss << t;
    return oss.str();
}
// 4 - nothing available
template <typename T, typename std::enable_if<!has_std_to_string<T>::value && !has_to_string<T>::value &&
                                                  !has_ostringstream<T>::value,
                                              int>::type = 0>
std::string make_string(const T&)
{
    return "??";
}

template <typename T, typename U>
std::string make_string(const std::pair<T, U>& p)
{
    return "{" + to_string(p.first) + ", " + to_string(p.second) + "}";
}

template <typename... Types>
std::string make_string(const std::tuple<Types...>& p)
{
    std::string s = "{";
    hpp::for_each(p, [&s](const auto& el) {
        s += to_string(el);
        s += ", ";
    });
    if(s.back() != '{')
    {
        s.pop_back();
        s.pop_back();
    }
    s += "}";
    return s;
}

inline std::string make_string(const std::string& p)
{
    return "\"" + p + "\"";
}
}
