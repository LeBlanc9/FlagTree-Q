set(LLVM_TARGET_DEFINITIONS QuantaDialect.td)
mlir_tablegen(QuantaDialect.h.inc -gen-dialect-decls -dialect=quanta)
mlir_tablegen(QuantaDialect.cpp.inc -gen-dialect-defs -dialect=quanta)
add_public_tablegen_target(FlagTreeQQuantaDialectIncGen)

set(LLVM_TARGET_DEFINITIONS QuantaTypes.td)
mlir_tablegen(QuantaTypes.h.inc -gen-typedef-decls -typedefs-dialect=quanta)
mlir_tablegen(QuantaTypes.cpp.inc -gen-typedef-defs -typedefs-dialect=quanta)
add_public_tablegen_target(FlagTreeQQuantaTypesIncGen)

set(LLVM_TARGET_DEFINITIONS QuantaOps.td)
mlir_tablegen(QuantaOps.h.inc -gen-op-decls)
mlir_tablegen(QuantaOps.cpp.inc -gen-op-defs)
add_public_tablegen_target(FlagTreeQQuantaOpsIncGen)
