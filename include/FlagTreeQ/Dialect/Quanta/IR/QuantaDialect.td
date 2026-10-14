#ifndef FLAGTREEQ_DIALECT_QUANTA_IR_QUANTADIALECT_TD
#define FLAGTREEQ_DIALECT_QUANTA_IR_QUANTADIALECT_TD

include "mlir/IR/DialectBase.td"

def Quanta_Dialect : Dialect {
  let name = "quanta";
  let cppNamespace = "::flagtreeq::quanta";
  let summary = "Quantum program representation for FlagTree-Q";
  let description = [{
    Quanta represents quantum programs alongside MLIR's classical dialects.
    It provides single-qubit references and unitary gates with optional
    quantum controls and adjoint modifiers, plus allocation, Z measurement,
    reset, and explicit deallocation. Linear quantum value types will be
    introduced separately.
  }];
  let useDefaultTypePrinterParser = 1;
  let extraClassDeclaration = [{
    void registerTypes();
  }];
}

#endif
