option(AUDIO_TEST_WITH_RASPA "Build with Raspa realtime audio (if false, will use dummy audio)" ON)

set(INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/third-party"
                 "${PROJECT_SOURCE_DIR}/third-party/optionparser" )

add_executable(raspa_audio_test main.cpp audio_test.cpp)
target_include_directories(raspa_audio_test PRIVATE ${INCLUDE_DIRS})
target_link_libraries(raspa_audio_test PRIVATE pthread nlohmann_json::nlohmann_json)
target_compile_features(raspa_audio_test PRIVATE cxx_std_20)
target_compile_options(raspa_audio_test PRIVATE -Wall -ffast-math -feliminate-unused-debug-types -Wno-psabi)


if (AUDIO_TEST_WITH_RASPA)
    target_link_libraries(raspa_audio_test PUBLIC raspa asound)
else()
    target_compile_definitions(raspa_audio_test PUBLIC DUMMY_AUDIO=1)
endif()

