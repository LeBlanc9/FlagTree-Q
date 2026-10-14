add_llvm_executable(flagtree-q-opt flagtree-q-opt.cpp)
llvm_update_compile_flags(flagtree-q-opt)
target_link_libraries(flagtree-q-opt PRIVATE
  FlagTreeQQuanta
  MLIRArithDialect
  MLIRFuncDialect
  MLIRMathDialect
  MLIRSCFDialect
  MLIROptLib
)
mlir_check_all_link_libraries(flagtree-q-opt)

install(TARGETS flagtree-q-opt RUNTIME DESTINATION bin)
