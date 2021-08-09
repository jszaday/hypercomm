set ( CORE_SOURCES locality.cc module.cc )

add_library(hypercomm-core "${CORE_SOURCES}")

target_include_directories(
    hypercomm-core PUBLIC "${CMAKE_BINARY_DIR}/include"
)

target_include_directories(
    hypercomm-core PUBLIC "${CMAKE_SOURCE_DIR}/include"
)

add_dependencies(hypercomm-core hypercomm-messaging)

add_dependencies(hypercomm-core locality_header)

add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/include/hypercomm/core/locality.decl.h"
    COMMAND ${CMAKE_CXX_COMPILER} "${CMAKE_SOURCE_DIR}/src/hypercomm/core/locality.ci"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/include/hypercomm/core
)

add_custom_target(locality_header ALL DEPENDS "${CMAKE_BINARY_DIR}/include/hypercomm/core/locality.decl.h")
