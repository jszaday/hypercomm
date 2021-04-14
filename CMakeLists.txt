cmake_minimum_required(VERSION 3.10)

project(hypercomm VERSION "0.0.001")

set(CMAKE_C_COMPILER "$(CHARM_HOME)/bin/charmc")
set(CMAKE_CXX_COMPILER "$(CHARM_HOME)/bin/charmc")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(src)
