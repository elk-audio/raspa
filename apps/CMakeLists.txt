##################
#  Source Files  #
##################

find_package(SndFile CONFIG REQUIRED)

set(LOOPBACK_SOURCE_FILES loopback.c)
set(TEST_TONE_SOURCE_FILES test_tone.c)
set(LOAD_TEST_SOURCE_FILES load_test.c)
set(SIGNAL_RECORDER_SOURCE_FILES signal_recorder/signal_recorder.cpp)

add_executable(raspa_loopback ${LOOPBACK_SOURCE_FILES})
add_executable(raspa_test_tone ${TEST_TONE_SOURCE_FILES})
add_executable(raspa_load_test ${LOAD_TEST_SOURCE_FILES})
add_executable(raspa_signal_recorder ${SIGNAL_RECORDER_SOURCE_FILES})

target_link_libraries(raspa_loopback PRIVATE raspa)
target_link_libraries(raspa_test_tone PRIVATE raspa)
target_link_libraries(raspa_load_test PRIVATE raspa)
target_link_libraries(raspa_signal_recorder PRIVATE raspa SndFile::sndfile)
