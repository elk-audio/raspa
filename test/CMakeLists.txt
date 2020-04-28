#####################################
#  Unit Tests Targets               #
#####################################

# build gtest framework
add_subdirectory(gtest)
enable_testing()

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
set_property(TARGET test_runner PROPERTY CXX_STANDARD 17)

add_test(test_runner unit_tests)


add_custom_target(run_tests ALL
    COMMAND "./test_runner"
)
add_dependencies(run_tests test_runner)
