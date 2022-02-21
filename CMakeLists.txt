cmake_minimum_required(VERSION 3.10)

project(hypercomm VERSION "0.0.001")

set(CHARMC_PATH $ENV{CHARM_HOME})
if(CHARMC_PATH)
    message(STATUS "Charm compiler found at: ${CHARMC_PATH}")
else(CHARMC_PATH)
    message(FATAL_ERROR "Charm compiler not found, please update the environment variable CHARM_HOME to the right location.")
endif(CHARMC_PATH)

set(CMAKE_C_COMPILER "${CHARMC_PATH}/bin/charmc")
set(CMAKE_CXX_COMPILER "${CHARMC_PATH}/bin/charmc")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include/hypercomm/core)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include/hypercomm/messaging)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include/hypercomm/tasking)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include/hypercomm/tree_builder)

if(EXISTS ${CMAKE_SOURCE_DIR}/parallel-hashmap)
    set(HYPERCOMM_USE_PHMAP yes)
    add_compile_definitions(HYPERCOMM_USE_PHMAP=1)
    include_directories(${CMAKE_SOURCE_DIR}/parallel-hashmap)
    message(STATUS "Hypercomm detected parallel-hashmap")
else()
    set(HYPERCOMM_USE_PHMAP no)
endif()

add_subdirectory(src)

# Generate template for tests
configure_file(Makefile.common.in ${CMAKE_SOURCE_DIR}/Makefile.common)
