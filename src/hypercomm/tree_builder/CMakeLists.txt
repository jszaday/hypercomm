add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/include/hypercomm/tree_builder/tree_builder.decl.h"
    COMMAND ${CMAKE_CXX_COMPILER} "${CMAKE_SOURCE_DIR}/src/hypercomm/tree_builder/tree_builder.ci"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/include/hypercomm/tree_builder
)

add_custom_target(tree_builder_header ALL DEPENDS "${CMAKE_BINARY_DIR}/include/hypercomm/tree_builder/tree_builder.decl.h")
