cmake_minimum_required(VERSION 3.22)
project(oledgui)
include_directories(src)

# To make clangd happy
SET(CMAKE_CXX_FLAGS "-std=c++2a -Werror -fconcepts-diagnostics-depth=10")

add_executable("ncurses" test/ncurses.cc )
target_link_libraries("ncurses" ncursesw)

add_executable("terminal" test/terminal.cc )

add_executable("test" test/test.cc)
target_link_libraries("test" ncursesw)
