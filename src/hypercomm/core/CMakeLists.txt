add_custom_command(
    OUTPUT "${PROJECT_SOURCE_DIR}/include/hypercomm/core/locality.decl.h"
    COMMAND "$(CHARM_HOME)/bin/charmc" -I. "${PROJECT_SOURCE_DIR}/src/hypercomm/core/locality.ci"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/include/hypercomm/core
)

add_custom_target(locality_header ALL DEPENDS "${PROJECT_SOURCE_DIR}/include/hypercomm/core/locality.decl.h")

# add_dependencies(${PROJECT_NAME} locality_header)
