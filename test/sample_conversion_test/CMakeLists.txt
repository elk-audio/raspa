set(CONVERSION_TEST_SOURCE_FILES sample_conversion_test.cpp sample_conv.cpp sample_conv.h)

add_executable(sample_conversion_test ${CONVERSION_TEST_SOURCE_FILES})

target_compile_options(sample_conversion_test PRIVATE -Wall -Wextra -Wno-psabi -fno-rtti -ffast-math)
target_include_directories(sample_conversion_test PRIVATE ${CMAKE_SOURCE_DIR}/src/sample_conversion)
target_include_directories(sample_conversion_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(sample_conversion_test PRIVATE -DRASPA_N_INPUT_CHANNELS=${RASPA_N_INPUT_CHANNELS}
                                                          -DRASPA_N_OUTPUT_CHANNELS=${RASPA_N_OUTPUT_CHANNELS})
