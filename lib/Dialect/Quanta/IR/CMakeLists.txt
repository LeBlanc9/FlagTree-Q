add_mlir_dialect_library(FlagTreeQQuanta
  QuantaDialect.cpp
  QuantaOps.cpp
  QuantaTypes.cpp

  DEPENDS
  FlagTreeQQuantaDialectIncGen
  FlagTreeQQuantaOpsIncGen
  FlagTreeQQuantaTypesIncGen

  LINK_LIBS PUBLIC
  MLIRIR
  MLIRFunctionInterfaces
  MLIRSideEffectInterfaces
)
