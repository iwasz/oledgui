# SPDX-License-Identifier: Apache-2.0

# list(APPEND ZEPHYR_EXTRA_MODULES
#   ${CMAKE_CURRENT_SOURCE_DIR}/modules/hw_timer
# )

cmake_minimum_required(VERSION 3.22.2)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
include(${ZEPHYR_BASE}/cmake/cfb.cmake NO_POLICY_SCOPE)
project(zephyr-test)

include_directories (src)
include_directories (../../../src)

target_sources(app PRIVATE 
    src/main.cc
)

set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)
generate_cfb_font_for_target(app src/fonts/oldschool-black-format.png ${gen_dir}/cfb_font_oldschool.h 7 8 --first 32 --last 126)
