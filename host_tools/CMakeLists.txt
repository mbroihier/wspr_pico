cmake_minimum_required(VERSION 3.12)
project(build_bit_patterns)
set(BUILD_BIT_PATTERNS_SRC build_bit_patterns_main.cc build_bit_patterns.cc)
add_executable(build_bit_patterns ${BUILD_BIT_PATTERNS_SRC})
target_link_libraries(build_bit_patterns stdc++ m)

