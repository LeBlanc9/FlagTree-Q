#ifndef FLAGTREEQ_TARGET_QIR_EXPORT_H
#define FLAGTREEQ_TARGET_QIR_EXPORT_H

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/Support/raw_ostream.h"

namespace flagtreeq::qir {

// Translate an already lowered QIR 2.0 Base Profile module in LLVM dialect to
// LLVM IR or bitcode. Does not run lowering passes or modify the source IR.
// The caller is responsible for profile/QIS legalization before export; this
// checks the version/resource flags and LLVM validity, not full QIR compliance.
mlir::LogicalResult exportQIR(mlir::ModuleOp module, llvm::raw_ostream &output,
                              bool emitBitcode = false);

} // namespace flagtreeq::qir

#endif
