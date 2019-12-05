##################
#  Source Files  #
##################

set(LOOPBACK_SOURCE_FILES loopback.c)

add_executable(loopback ${LOOPBACK_SOURCE_FILES})
target_link_libraries(loopback PRIVATE raspa)