cmake_minimum_required(VERSION 3.22)
project(oledgui)
include_directories(src)

# To make clangd happy
SET(CMAKE_CXX_FLAGS "-std=c++2a -Werror -fconcepts-diagnostics-depth=10")

add_executable("demo"
    test/demo.cc
)

target_link_libraries("demo" ncursesw)

add_executable("test"
    test/test.cc
)

target_link_libraries("test" ncursesw)
