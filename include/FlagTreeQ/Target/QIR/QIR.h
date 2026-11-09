#ifndef FLAGTREEQ_TARGET_QIR_QIR_H
#define FLAGTREEQ_TARGET_QIR_QIR_H

#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/BuiltinOps.h"
#include <cstdint>

namespace llvm {
class Module;
}

namespace flagtreeq::qir {

// QIR 2.0 Base Profile. The QIS below is our supported instruction set, not a
// gate set mandated by the QIR specification. Consumers must support it too.
inline constexpr unsigned majorVersion = 2;
inline constexpr unsigned minorVersion = 0;
inline constexpr llvm::StringLiteral baseProfile = "base_profile";
inline constexpr llvm::StringLiteral outputLabelingSchema = "flagtree_q_v1";

enum class Function {
  H,
  X,
  Y,
  Z,
  S,
  SAdj,
  T,
  TAdj,
  RX,
  RY,
  RZ,
  Swap,
  CNOT,
  CZ,
  CCX,
  MZ,
  Initialize,
  RecordResult,
  RecordTuple
};

llvm::StringRef getFunctionName(Function function);

// The destination is a fresh module owned by lowering; existing declarations
// must have been created by this helper with the same QIR ABI.
mlir::LLVM::LLVMFuncOp getOrCreateDeclaration(mlir::ModuleOp module,
                                              Function function);
void setEntryPointAttributes(mlir::LLVM::LLVMFuncOp entry, uint64_t qubits,
                             uint64_t results);
void addModuleFlags(mlir::ModuleOp module);
// The pinned MLIR exporter widens integer module flags to i32. Restore the i1
// flags required by QIR after translating to LLVM IR.
void finalizeModuleFlags(llvm::Module &module);
mlir::Value createStaticHandle(mlir::OpBuilder &builder,
                               mlir::Location location, uint64_t index);

} // namespace flagtreeq::qir

#endif
