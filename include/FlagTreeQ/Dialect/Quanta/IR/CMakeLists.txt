set(LLVM_TARGET_DEFINITIONS QuantaDialect.td)
mlir_tablegen(QuantaDialect.h.inc -gen-dialect-decls -dialect=quanta)
mlir_tablegen(QuantaDialect.cpp.inc -gen-dialect-defs -dialect=quanta)
add_public_tablegen_target(FlagTreeQQuantaDialectIncGen)
