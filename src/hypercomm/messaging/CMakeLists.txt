set ( MESSAGING_SOURCES messaging.cc )

add_library(hypercomm-messaging "${MESSAGING_SOURCES}" "${PROJECT_SOURCE_DIR}/include/hypercomm/messaging/messaging.decl.h")

target_include_directories(
    hypercomm-messaging PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

add_custom_command(
    OUTPUT "${PROJECT_SOURCE_DIR}/include/hypercomm/messaging/messaging.decl.h"
    COMMAND "$(CHARM_HOME)/bin/charmc" -I. "${PROJECT_SOURCE_DIR}/src/hypercomm/messaging/messaging.ci"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/include/hypercomm/messaging
)
