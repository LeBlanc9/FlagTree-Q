add_mlir_library(FlagTreeQQuantaToQIR
  QuantaToQIR.cpp
  LINK_LIBS PUBLIC
  FlagTreeQQuanta FlagTreeQQIR
  MLIRArithDialect MLIRFuncDialect MLIRLLVMDialect MLIRPass MLIRTransforms
)
