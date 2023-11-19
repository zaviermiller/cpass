cmake_minimum_required(VERSION 3.1)
Project(test_copyprop)

# support C++14 features used by LLVM 10.0.0
set(CMAKE_CXX_STANDARD 14)

find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

add_subdirectory(copy_prop)  # Use your pass name here.
