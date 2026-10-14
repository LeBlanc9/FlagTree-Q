set(LLVM_LIT_ARGS "-sv" CACHE STRING "Default options for lit")
find_program(FLAGTREE_Q_LIT NAMES llvm-lit lit HINTS "${LLVM_TOOLS_BINARY_DIR}")
find_program(FLAGTREE_Q_FILECHECK NAMES FileCheck HINTS "${LLVM_TOOLS_BINARY_DIR}")
if(NOT FLAGTREE_Q_LIT OR NOT FLAGTREE_Q_FILECHECK)
  message(FATAL_ERROR
    "Tests require llvm-lit (or lit) and FileCheck. Set FLAGTREE_Q_LIT and "
    "FLAGTREE_Q_FILECHECK, or configure with FLAGTREE_Q_BUILD_TESTS=OFF.")
endif()
set(LLVM_EXTERNAL_LIT "${FLAGTREE_Q_LIT}" CACHE STRING "Path to lit")

configure_lit_site_cfg(
  "${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in"
  "${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py"
  MAIN_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/lit.cfg.py"
)

add_lit_testsuite(check-flagtree-q-lit "Running FlagTree-Q MLIR tests"
  "${CMAKE_CURRENT_BINARY_DIR}"
  DEPENDS flagtree-q-opt
)
add_test(NAME flagtree-q-lit
  COMMAND "${Python3_EXECUTABLE}" "${FLAGTREE_Q_LIT}" -sv "${CMAKE_CURRENT_BINARY_DIR}"
)

set(FLAGTREE_Q_PYTHON_TEST_COMMAND
  "${CMAKE_COMMAND}" -E env
  "PYTHONPATH=${PROJECT_SOURCE_DIR}/python"
  "FLAGTREE_Q_OPT=$<TARGET_FILE:flagtree-q-opt>"
  "${Python3_EXECUTABLE}" -m unittest discover
  -s "${CMAKE_CURRENT_SOURCE_DIR}/python" -v
)
add_test(NAME flagtree-q-python COMMAND ${FLAGTREE_Q_PYTHON_TEST_COMMAND})
add_custom_target(check-flagtree-q-python
  COMMAND ${FLAGTREE_Q_PYTHON_TEST_COMMAND}
  DEPENDS flagtree-q-opt
  USES_TERMINAL
)
add_custom_target(check-flagtree-q
  DEPENDS check-flagtree-q-lit check-flagtree-q-python
)
