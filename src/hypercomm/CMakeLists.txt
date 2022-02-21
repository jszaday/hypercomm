add_subdirectory(core)
add_subdirectory(messaging)
add_subdirectory(components)
add_subdirectory(tasking)
add_subdirectory(tree_builder)
add_subdirectory(serialization)

set ( UTILITIES_SOURCES utilities.cc )

add_library(hypercomm-utilities "${UTILITIES_SOURCES}")

add_dependencies(hypercomm-utilities hypercomm-messaging)

target_include_directories(
    hypercomm-utilities PUBLIC "${CMAKE_BINARY_DIR}/include"
)

target_include_directories(
    hypercomm-utilities PUBLIC "${CMAKE_SOURCE_DIR}/include"
)
