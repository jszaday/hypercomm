add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/include/hypercomm/core/locality.decl.h"
    COMMAND ${CMAKE_CXX_COMPILER} "${CMAKE_SOURCE_DIR}/src/hypercomm/core/locality.ci"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/include/hypercomm/core
)

add_custom_target(locality_header ALL DEPENDS "${CMAKE_BINARY_DIR}/include/hypercomm/core/locality.decl.h")
