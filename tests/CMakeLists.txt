message(STATUS "Enabled tests.")

set(target_name dynopp_test)

file(GLOB_RECURSE libsrc *.h *.cpp *.hpp *.c *.cc)

add_executable(${target_name} ${libsrc})

target_link_libraries(${target_name} PUBLIC dynopp suitepp)

set_target_properties(${target_name} PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

include(target_warning_support)
set_warning_level(${target_name} ultra)

enable_testing()
add_test(dynopp_test ${target_name})
