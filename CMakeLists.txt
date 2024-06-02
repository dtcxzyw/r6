cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
set(CMAKE_FIND_PACKAGE_SORT_DIRECTION DEC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(r6)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
include(AddLLVM)

include_directories(${LLVM_INCLUDE_DIRS})
set(LLVM_LINK_COMPONENTS core support irreader irprinter analysis instcombine passes)
add_llvm_executable(constextract PARTIAL_SOURCES_INTENDED constextract.cpp)
add_llvm_executable(constmat PARTIAL_SOURCES_INTENDED constmat.cpp)
add_llvm_executable(costestimate PARTIAL_SOURCES_INTENDED costestimate.cpp)
