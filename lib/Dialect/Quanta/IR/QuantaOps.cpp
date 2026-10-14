#include "FlagTreeQ/Dialect/Quanta/IR/QuantaOps.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/Interfaces/FunctionInterfaces.h"
#include "llvm/ADT/DenseSet.h"

using namespace mlir;

namespace {

LogicalResult verifyNonUnitaryOp(Operation *op) {
  if (op->hasAttr("is_adj"))
    return op->emitOpError("does not support the 'is_adj' modifier");
  return success();
}

LogicalResult verifyGate(Operation *op, ValueRange parameters,
                         ValueRange controls, ValueRange targets,
                         unsigned expectedParameters,
                         unsigned expectedTargets) {
  if (parameters.size() != expectedParameters)
    return op->emitOpError("expects ")
           << expectedParameters << " parameter(s), got " << parameters.size();
  if (targets.size() != expectedTargets)
    return op->emitOpError("expects ")
           << expectedTargets << " target(s), got " << targets.size();

  llvm::SmallDenseSet<Value, 4> qubits;
  for (Value control : controls)
    if (!qubits.insert(control).second)
      return op->emitOpError("control qubits must be distinct");
  for (Value target : targets)
    if (!qubits.insert(target).second)
      return op->emitOpError("target qubits must be distinct from each other "
                             "and from controls");
  return success();
}

} // namespace

#define GET_OP_CLASSES
#include "FlagTreeQ/Dialect/Quanta/IR/QuantaOps.cpp.inc"

using namespace flagtreeq::quanta;

LogicalResult AllocOp::verify() {
  return verifyNonUnitaryOp(getOperation());
}

LogicalResult MeasureOp::verify() {
  return verifyNonUnitaryOp(getOperation());
}

LogicalResult ResetOp::verify() {
  return verifyNonUnitaryOp(getOperation());
}

LogicalResult DeallocOp::verify() {
  if (failed(verifyNonUnitaryOp(getOperation())))
    return failure();

  if (auto argument = dyn_cast<BlockArgument>(getQubit())) {
    Block *block = argument.getOwner();
    if (block->isEntryBlock() &&
        isa<FunctionOpInterface>(block->getParentOp()))
      return emitOpError("cannot deallocate a borrowed function argument");
  }

  // Only diagnose direct uses with a known order in the same block.
  for (Operation *user : getQubit().getUsers()) {
    if (user != getOperation() && user->getBlock() == getOperation()->getBlock() &&
        getOperation()->isBeforeInBlock(user))
      return emitOpError("qubit is used after this deallocation in the same "
                         "block (including duplicate deallocation)");
  }
  return success();
}
