add_mlir_dialect_library(FlagTreeQQuanta
  QuantaDialect.cpp

  DEPENDS
  FlagTreeQQuantaDialectIncGen

  LINK_LIBS PUBLIC
  MLIRIR
)
