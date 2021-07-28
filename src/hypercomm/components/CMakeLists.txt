set ( COMPONENT_SOURCES component.cc )

add_library(hypercomm-components "${COMPONENT_SOURCES}")

add_dependencies(hypercomm-components hypercomm-messaging)

target_include_directories(
    hypercomm-components PUBLIC
    "${CMAKE_BINARY_DIR}/include"
)

target_include_directories(
    hypercomm-components PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
)
