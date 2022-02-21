add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/include/hypercomm/tasking/tasking.decl.h"
    COMMAND ${CMAKE_CXX_COMPILER} "${CMAKE_SOURCE_DIR}/src/hypercomm/tasking/tasking.ci"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/include/hypercomm/tasking
)

add_custom_target(tasking_header ALL DEPENDS "${CMAKE_BINARY_DIR}/include/hypercomm/tasking/tasking.decl.h")
