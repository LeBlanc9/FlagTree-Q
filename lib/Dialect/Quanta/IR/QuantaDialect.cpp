#include "FlagTreeQ/Dialect/Quanta/IR/QuantaDialect.h"
#include "FlagTreeQ/Dialect/Quanta/IR/QuantaOps.h"

#include "FlagTreeQ/Dialect/Quanta/IR/QuantaDialect.cpp.inc"

void flagtreeq::quanta::QuantaDialect::initialize() {
  registerTypes();
  addOperations<
#define GET_OP_LIST
#include "FlagTreeQ/Dialect/Quanta/IR/QuantaOps.cpp.inc"
      >();
}
