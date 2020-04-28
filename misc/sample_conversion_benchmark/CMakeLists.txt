cmake_minimum_required(VERSION 3.8)
project(sample_conversion_benchmark)

set(CONVERSION_TEST_SOURCE_FILES sample_conversion_benchmark.cpp)

add_executable(sample_conversion_benchmark ${CONVERSION_TEST_SOURCE_FILES})

target_compile_options(sample_conversion_benchmark PRIVATE -Wall -Wextra -Wno-psabi -fno-rtti -ffast-math -fno-exceptions -O3)
target_include_directories(sample_conversion_benchmark PRIVATE ${CMAKE_SOURCE_DIR}/../../src)
set_property(TARGET sample_conversion_benchmark PROPERTY CXX_STANDARD 17)
