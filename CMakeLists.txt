cmake_minimum_required(VERSION 3.18)
project(raspalib)

#################################
#  Build configuration options  #
#################################

# These can be overriden by ccmake, commandline args or using
# set(BUILD_OPTION <new value> CACHE <type> "") in an including project

option(RASPA_WITH_APPS "Build included applications" ON)
option(RASPA_WITH_TESTS "Build and run unit tests" OFF)
option(RASPA_WITH_EVL "Build Raspa for EVL based drivers" ON)

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

set(RASPALIB_EXTRA_CLION_SOURCES src/driver_config.h
                                 src/raspa_error_codes.h
                                 src/raspa_pimpl.h
                                 src/sample_conversion.h)

set(RASPALIB_COMPILATION_UNITS src/raspa_api_wrapper.cpp)

set(RASPALIB_SOURCE_FILES "${RASPALIB_COMPILATION_UNITS}" "${RASPALIB_EXTRA_CLION_SOURCES}")

add_library(raspa STATIC ${RASPALIB_SOURCE_FILES})

if (${RASPA_WITH_EVL})
    target_compile_definitions(raspa PUBLIC -DRASPA_WITH_EVL)
    target_link_libraries(raspa PRIVATE evl)
else()
    add_xenomai_to_target(raspa)
endif()

target_include_directories(raspa PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(raspa PRIVATE ${PROJECT_SOURCE_DIR}/src/)
set_property(TARGET raspa PROPERTY CXX_STANDARD 17)
target_compile_options(raspa PRIVATE -Wall -Wextra -ffast-math -feliminate-unused-debug-types -fno-exceptions)

set(RASPALIB_LINKED_LIBS pthread audio_control_protocol fifo asound)

target_link_libraries(raspa PRIVATE ${RASPALIB_LINKED_LIBS})

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
add_subdirectory(audio-control-protocol)
add_subdirectory(third-party/fifo)

if (${RASPA_WITH_APPS})
    add_subdirectory(apps)
endif()
if (${RASPA_WITH_TESTS})
    add_subdirectory(test)
endif()
