#ifndef FLAGTREEQ_DIALECT_QUANTA_IR_QUANTAOPS_H
#define FLAGTREEQ_DIALECT_QUANTA_IR_QUANTAOPS_H

#include "FlagTreeQ/Dialect/Quanta/IR/QuantaTypes.h"
#include "mlir/Bytecode/BytecodeOpInterface.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

#define GET_OP_CLASSES
#include "FlagTreeQ/Dialect/Quanta/IR/QuantaOps.h.inc"

#endif
