#ifndef FLAGTREEQ_DIALECT_QUANTA_IR_QUANTADIALECT_TD
#define FLAGTREEQ_DIALECT_QUANTA_IR_QUANTADIALECT_TD

include "mlir/IR/DialectBase.td"

def Quanta_Dialect : Dialect {
  let name = "quanta";
  let cppNamespace = "::flagtreeq::quanta";
  let summary = "Quantum program representation for FlagTree-Q";
  let description = [{
    Quanta will represent quantum values and operations alongside MLIR's
    classical dialects. This initial skeleton registers the dialect only;
    quantum types, operations, and their semantics are not implemented yet.
  }];
}

#endif
