#ifndef FLAGTREEQ_DIALECT_QUANTA_IR_QUANTATYPES_TD
#define FLAGTREEQ_DIALECT_QUANTA_IR_QUANTATYPES_TD

include "FlagTreeQ/Dialect/Quanta/IR/QuantaDialect.td"
include "mlir/IR/AttrTypeBase.td"

def Quanta_QubitType : TypeDef<Quanta_Dialect, "Qubit"> {
  let mnemonic = "qubit";
  let hasCustomAssemblyFormat = 1;
  let summary = "Reference to a single logical qubit";
  let description = [{
    A reference to a single logical qubit, written as `!quanta.qubit`. It has no
    parameters and encodes neither a physical index nor a state vector. Passing
    the reference forwards access to the same qubit; it does not copy the state.

    This is a reference type, not a linear value type. Gates update the referenced
    state through side effects. Allocation creates an owning reference; function
    arguments borrow; deallocation invalidates every reference to the same qubit.
  }];
}

#endif
