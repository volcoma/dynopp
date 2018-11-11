file(GLOB_RECURSE libsrc *.h *.cpp *.hpp *.c *.cc)

set(target_name dynopp)

add_library(${target_name} ${libsrc})

target_include_directories(${target_name}
    PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(${target_name} PUBLIC hpp)

set_target_properties(${target_name} PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

include(target_warning_support)
set_warning_level(${target_name} ultra)

include(target_code_style_support)
set_code_style(${target_name} lower_case check_headers "${extra_flags}")
