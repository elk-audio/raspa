#####################################
#  Unit Tests Targets               #
#####################################

# build gtest framework
add_subdirectory(gtest)
enable_testing()

#####################
#  Add other tests  #
#####################

add_subdirectory(sample_conversion_test EXCLUDE_FROM_ALL)

#####################
#  Unit Test Files  #
#####################

SET(TEST_FILES
    unittests/sample_conversion_test.cpp
)

##########################################
#  Include directories relative to test  #
##########################################

# Don't reuse the one defined on top-level CMakeLists,
# as Cobalt redefines some Posix stuff that is incompatible with GTest
set(TEST_INCLUDE_DIRS
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/sample_conversion
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/test/unittests
    ${PROJECT_SOURCE_DIR}/test/gtest/include
)

#################################
#  Statically linked libraries  #
#################################

set(TEST_LINK_LIBRARIES
    gtest
    gtest_main
    m
)

add_executable(test_runner ${TEST_FILES})

target_compile_options(test_runner PRIVATE -DDISABLE_LOGGING)
target_include_directories(test_runner PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_runner PRIVATE ${TEST_LINK_LIBRARIES})

target_compile_definitions(test_runner PUBLIC -DRASPA_N_INPUT_CHANNELS=${RASPA_N_INPUT_CHANNELS}
                                              -DRASPA_N_OUTPUT_CHANNELS=${RASPA_N_OUTPUT_CHANNELS})
add_test(test_runner unit_tests)


add_custom_target(run_tests ALL
    COMMAND "./test_runner"
)
add_dependencies(run_tests test_runner)