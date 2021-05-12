set ( SERIALIZATION_SOURCES enrollment.cc )

add_library(hypercomm-serialization "${SERIALIZATION_SOURCES}")

target_include_directories(
    hypercomm-serialization PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)
