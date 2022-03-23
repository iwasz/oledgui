cmake_minimum_required(VERSION 3.22)
project(oledgui)
include_directories(src)

# To make clangd happy
SET(CMAKE_CXX_FLAGS "-std=c++2a")

add_executable(${CMAKE_PROJECT_NAME}
    test/main.cc
)

target_link_libraries(${CMAKE_PROJECT_NAME} ncursesw)
