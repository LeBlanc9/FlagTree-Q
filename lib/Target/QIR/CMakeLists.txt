add_mlir_library(FlagTreeQQIR
  QIR.cpp
  PARTIAL_SOURCES_INTENDED
  LINK_COMPONENTS Core
  LINK_LIBS PUBLIC MLIRLLVMDialect MLIRIR
)

add_mlir_library(FlagTreeQQIRExport
  Export.cpp
  PARTIAL_SOURCES_INTENDED
  LINK_COMPONENTS BitWriter Core
  LINK_LIBS PUBLIC
  FlagTreeQQIR
  MLIRBuiltinToLLVMIRTranslation
  MLIRLLVMToLLVMIRTranslation
  MLIRTargetLLVMIRExport
)
