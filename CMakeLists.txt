cmake_minimum_required(VERSION 3.20)
project(FlagTreeQ VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_WITH_INSTALL_NAME_DIR ON)

find_package(MLIR REQUIRED CONFIG)
message(STATUS "Using MLIR from: ${MLIR_DIR}")
message(STATUS "LLVM version: ${LLVM_PACKAGE_VERSION}")

list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}" "${LLVM_CMAKE_DIR}")
include(TableGen)
include(AddLLVM)
include(AddMLIR)
include(HandleLLVMOptions)

set(LLVM_RUNTIME_OUTPUT_INTDIR "${PROJECT_BINARY_DIR}/bin")
set(LLVM_LIBRARY_OUTPUT_INTDIR "${PROJECT_BINARY_DIR}/lib")
set(MLIR_BINARY_DIR "${PROJECT_BINARY_DIR}")

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS} ${MLIR_INCLUDE_DIRS})
include_directories("${PROJECT_SOURCE_DIR}/include" "${PROJECT_BINARY_DIR}/include")
add_definitions(${LLVM_DEFINITIONS})

add_subdirectory(include)
add_subdirectory(lib)
add_subdirectory(tools)

option(FLAGTREE_Q_BUILD_TESTS "Build FlagTree-Q regression tests" ON)
if(FLAGTREE_Q_BUILD_TESTS)
  enable_testing()
  find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
  add_subdirectory(test)
endif()
