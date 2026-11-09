find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter Development.Module)
set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 2.13 CONFIG QUIET)
if(NOT pybind11_FOUND)
  execute_process(
    COMMAND "${Python3_EXECUTABLE}" -m pybind11 --cmakedir
    OUTPUT_VARIABLE FLAGTREE_Q_PYBIND11_CMAKE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
  )
  find_package(pybind11 2.13 CONFIG REQUIRED
    PATHS "${FLAGTREE_Q_PYBIND11_CMAKE_DIR}" NO_DEFAULT_PATH)
endif()

# Only the extension needs C++ exceptions and RTTI for Python interoperability.
set(LLVM_REQUIRES_EH ON)
set(LLVM_REQUIRES_RTTI ON)
pybind11_add_module(libflagtree_q MODULE NO_EXTRAS main.cpp ir.cpp)
llvm_update_compile_flags(libflagtree_q)
target_link_libraries(libflagtree_q PRIVATE
  FlagTreeQQuanta FlagTreeQQuantaToQIR FlagTreeQQIRExport
  MLIRArithDialect MLIRFuncDialect MLIRIR MLIRPass MLIRSupport)
mlir_check_all_link_libraries(libflagtree_q)

set(FLAGTREE_Q_PYTHON_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/python/flagtree_q/_C"
  CACHE PATH "Output directory for the native Python extension")
set_target_properties(libflagtree_q PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${FLAGTREE_Q_PYTHON_OUTPUT_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${FLAGTREE_Q_PYTHON_OUTPUT_DIR}")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  set_target_properties(libflagtree_q PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY_${config} "${FLAGTREE_Q_PYTHON_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_${config} "${FLAGTREE_Q_PYTHON_OUTPUT_DIR}")
endforeach()
