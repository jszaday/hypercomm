add_subdirectory(components)
add_subdirectory(serialization)

set ( UTILITIES_SOURCES utilities.cc )

add_library(hypercomm-utilities "${UTILITIES_SOURCES}")

target_include_directories(
    hypercomm-utilities PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)
