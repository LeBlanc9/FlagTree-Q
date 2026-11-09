add_llvm_executable(flagtree-q-translate flagtree-q-translate.cpp)
llvm_update_compile_flags(flagtree-q-translate)
target_link_libraries(flagtree-q-translate PRIVATE
  FlagTreeQQIRExport FlagTreeQQuantaToQIR FlagTreeQQuanta
  MLIRArithDialect MLIRFuncDialect MLIRLLVMDialect MLIRPass MLIRTranslateLib
)
mlir_check_all_link_libraries(flagtree-q-translate)
install(TARGETS flagtree-q-translate RUNTIME DESTINATION bin)
