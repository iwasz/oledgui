cmake_minimum_required(VERSION 3.22)
project(oledgui)
include_directories(src)

# To make clangd happy
# SET(CMAKE_CXX_FLAGS "-std=c++20 -Werror -fconcepts-diagnostics-depth=10 -ftemplate-backtrace-limit=0")
SET(CMAKE_CXX_FLAGS "-std=c++20 -Werror -fconcepts-diagnostics-depth=0 -ftemplate-backtrace-limit=0")

add_executable("ncurses" test/component/ncurses.cc )
target_link_libraries("ncurses" ncursesw)

add_executable("regression" 
    test/regression/regression.cc 
    test/regression/inputApi.cc
    test/regression/textWidget.cc
    test/regression/cellPhone.cc
    test/regression/layout.cc
)
target_link_libraries("regression" ncursesw)

add_executable("demo" test/demo/demo.cc )
target_link_libraries("demo" ncursesw)

add_executable("terminal" test/component/terminal.cc )

add_executable("test" test/unit/test.cc)
target_link_libraries("test" ncursesw)
