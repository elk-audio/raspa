cmake_minimum_required(VERSION 3.8)
project(raspalib)

#################################
#  Build configuration options  #
#################################

# These can be overriden by ccmake, commandline args or using
# set(BUILD_OPTION <new value> CACHE <type> "") in an including project

option(RASPA_WITH_APPS "Build included applications" OFF)
option(RASPA_WITH_TESTS "Build and run unit tests" ON)
option(RASPA_WITH_UCONTROLLER "Build for micro controller based platforms" OFF)
set(RASPA_N_INPUT_CHANNELS 8 CACHE STRING "Number of input hw channels")
set(RASPA_N_OUTPUT_CHANNELS 8 CACHE STRING "Number of output hw channels")

if(RASPA_N_INPUT_CHANNELS GREATER RASPA_N_OUTPUT_CHANNELS)
    message( "Num codec channels is ${RASPA_N_INPUT_CHANNELS}")
else()
    message( "Num codec channels is ${RASPA_N_OUTPUT_CHANNELS}")
endif()

if (${RASPA_WITH_UCONTROLLER})
    message("Building for micro-controller based platforms")
endif()

#######################
#  Cross compilation  #
#######################

set(XENOMAI_BASE_DIR "/usr/xenomai" CACHE STRING "xenomai base dir path")

if(NOT "$ENV{CMAKE_SYSROOT}" STREQUAL "")
    set(CMAKE_SYSROOT "$ENV{CMAKE_SYSROOT}")
    message("ENV_CMAKE_SYSROOT = " $ENV{CMAKE_SYSROOT})
endif()
if(NOT "${CMAKE_SYSROOT}" STREQUAL "")
    set(XENOMAI_BASE_DIR "${CMAKE_SYSROOT}/usr/xenomai")
    message("XENOMAI_BASE_DIR is " ${XENOMAI_BASE_DIR})
endif()

#######################
#  Utility functions  #
#######################

# Set Xenomai build options at local scope
# This intentionally does not include all the super-auto-magic linker wrappers,
# who seems to play very badly with other libraries at static initialization.
function(add_xenomai_to_target target)

    # from `xeno-config --skin=posix --cflags`
    set(XENOMAI_C_FLAGS "-D_GNU_SOURCE -D_REENTRANT -D__COBALT__ -D__COBALT_WRAP__")

    set(XENOMAI_INCLUDE_DIRS
        ${XENOMAI_BASE_DIR}/include
        ${XENOMAI_BASE_DIR}/include/cobalt
    )

    find_library(COBALT_LIB cobalt HINTS ${XENOMAI_BASE_DIR}/lib)
    target_compile_options(${target} PRIVATE ${XENOMAI_C_FLAGS})
    target_include_directories(${target} PRIVATE ${XENOMAI_INCLUDE_DIRS})
    target_link_libraries(${target} PRIVATE ${COBALT_LIB} rt m)
endfunction()

####################
#  Library target  #
####################

# Enumerate all the headers separately so that CLion can index them

set(RASPALIB_EXTRA_CLION_SOURCES src/driver_config.h)

if (${RASPA_WITH_UCONTROLLER})
    set(RASPALIB_COMPILATION_UNITS src/raspa_uc.c
                                   src/delay_error_filter.c
                                   src/com_task.c
                                   src/fifo.c)
    set(RASPALIB_EXTRA_CLION_SOURCES ${RASPALIB_EXTRA_CLION_SOURCES}
                                     src/delay_error_filter.h
                                     src/com_task.h
                                     src/fifo.h)
else()
    set(RASPALIB_COMPILATION_UNITS src/raspa.c)
endif()

set(RASPALIB_SOURCE_FILES "${RASPALIB_COMPILATION_UNITS}" "${RASPALIB_EXTRA_CLION_SOURCES}")

add_library(raspa STATIC ${RASPALIB_SOURCE_FILES})
add_xenomai_to_target(raspa)

target_include_directories(raspa PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(raspa PRIVATE ${PROJECT_SOURCE_DIR}/src/
                                         ${PROJECT_SOURCE_DIR}/src/sample_conversion)

if (${RASPA_WITH_UCONTROLLER})
    target_link_libraries(raspa PRIVATE pthread audio_control_protocol)
else()
    target_link_libraries(raspa PRIVATE pthread)
endif()

target_compile_features(raspa PUBLIC c_std_99)
target_compile_options(raspa PRIVATE -Wall -Wextra -Wno-discarded-qualifiers)

#Set compile macros on the target
target_compile_definitions(raspa PRIVATE -DRASPA_N_INPUT_CHANNELS=${RASPA_N_INPUT_CHANNELS}
                                         -DRASPA_N_OUTPUT_CHANNELS=${RASPA_N_OUTPUT_CHANNELS})

#############
#  Install  #
#############

set_target_properties(raspa PROPERTIES VERSION 0.1)
set_target_properties(raspa PROPERTIES PUBLIC_HEADER include/raspa/raspa.h)

install(TARGETS raspa
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/raspa
        )

#############################################
#  Subdirectory projects                    #
#############################################
if (${RASPA_WITH_APPS})
    add_subdirectory(apps)
endif()
if (${RASPA_WITH_TESTS})
    add_subdirectory(test)
endif()
if (${RASPA_WITH_UCONTROLLER})
    add_subdirectory(audio_control_protocol)
endif()