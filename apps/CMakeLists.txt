##################
#  Source Files  #
##################

set(LOOPBACK_SOURCE_FILES loopback.c)
set(TEST_TONE_SOURCE_FILES test_tone.c)
set(LOAD_TEST_SOURCE_FILES load_test.c)

add_executable(loopback ${LOOPBACK_SOURCE_FILES})
add_executable(test_tone ${TEST_TONE_SOURCE_FILES})
add_executable(load_test ${LOAD_TEST_SOURCE_FILES})

target_link_libraries(loopback PRIVATE raspa)
target_link_libraries(test_tone PRIVATE raspa)
target_link_libraries(load_test PRIVATE raspa)
