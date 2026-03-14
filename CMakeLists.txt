cmake_minimum_required(VERSION 3.16)

project(futex_mutex LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(futex_mutex INTERFACE)
target_include_directories(futex_mutex INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(futex_mutex_tests tests/futex_mutex_tests.cpp)
target_link_libraries(futex_mutex_tests PRIVATE futex_mutex)

enable_testing()
add_test(NAME futex_mutex_tests COMMAND futex_mutex_tests)
