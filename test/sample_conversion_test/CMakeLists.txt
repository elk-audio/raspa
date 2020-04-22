set(CONVERSION_TEST_SOURCE_FILES sample_conversion_test.cpp)

add_executable(sample_conversion_test ${CONVERSION_TEST_SOURCE_FILES})

target_compile_options(sample_conversion_test PRIVATE -Wall -Wextra -Wno-psabi -fno-rtti -ffast-math -fno-exceptions -O3)
target_include_directories(sample_conversion_test PRIVATE ${CMAKE_SOURCE_DIR}/src/sample_conversion)
target_include_directories(sample_conversion_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_property(TARGET sample_conversion_test PROPERTY CXX_STANDARD 17)
