#ifndef FLAGTREEQ_CONVERSION_QUANTATOQIR_PASSES_H
#define FLAGTREEQ_CONVERSION_QUANTATOQIR_PASSES_H

#include "mlir/Pass/Pass.h"
#include <memory>

namespace flagtreeq {

// Lower one closed, straight-line Quanta function to LLVM dialect QIR.
// Constant classical arithmetic is folded; unsupported programs are diagnosed.
std::unique_ptr<mlir::Pass> createConvertQuantaToQIRPass();
void registerQuantaToQIRPasses();

} // namespace flagtreeq

#endif
