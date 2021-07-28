set ( MESSAGING_SOURCES messaging.cc )

add_library(hypercomm-messaging "${MESSAGING_SOURCES}" "${CMAKE_BINARY_DIR}/include/hypercomm/messaging/messaging.decl.h")

add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/include/hypercomm/messaging/messaging.decl.h"
    COMMAND ${CMAKE_CXX_COMPILER} "${CMAKE_SOURCE_DIR}/src/hypercomm/messaging/messaging.ci"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/include/hypercomm/messaging
)

add_dependencies(hypercomm-messaging locality_header)

target_include_directories(
    hypercomm-messaging PUBLIC "${CMAKE_BINARY_DIR}/include"
)

target_include_directories(
    hypercomm-messaging PUBLIC "${CMAKE_SOURCE_DIR}/include"
)
