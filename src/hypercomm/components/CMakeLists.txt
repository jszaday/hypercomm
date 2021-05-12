set ( COMPONENT_SOURCES component.cc )

add_library(hypercomm-components "${COMPONENT_SOURCES}")

target_include_directories(
    hypercomm-components PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)
