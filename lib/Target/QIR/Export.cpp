#include "FlagTreeQ/Target/QIR/Export.h"
#include "FlagTreeQ/Target/QIR/QIR.h"
#include "mlir/IR/Verifier.h"
#include "mlir/Target/LLVMIR/Dialect/Builtin/BuiltinToLLVMIRTranslation.h"
#include "mlir/Target/LLVMIR/Dialect/LLVMIR/LLVMToLLVMIRTranslation.h"
#include "mlir/Target/LLVMIR/Export.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"

namespace {

// Reject a wrong compilation stage or incompatible ABI before finalizing the
// flag widths. Profile/QIS legalization remains the lowering stage's job.
mlir::LogicalResult checkLoweredModule(mlir::ModuleOp module) {
  auto result = module.walk([&](mlir::Operation *operation) {
    if (operation == module.getOperation() ||
        llvm::isa_and_nonnull<mlir::LLVM::LLVMDialect>(operation->getDialect()))
      return mlir::WalkResult::advance();
    operation->emitError(
        "QIR export requires an already lowered LLVM dialect module");
    return mlir::WalkResult::interrupt();
  });
  if (result.wasInterrupted())
    return mlir::failure();

  auto flags = module.getOps<mlir::LLVM::ModuleFlagsOp>();
  if (!llvm::hasSingleElement(flags))
    return module.emitError(
        "QIR export requires one llvm.module_flags operation");
  auto matches = [&](llvm::StringRef name, unsigned width, uint64_t expected) {
    unsigned count = 0;
    for (auto attribute : (*flags.begin()).getFlags()) {
      auto flag = mlir::dyn_cast<mlir::LLVM::ModuleFlagAttr>(attribute);
      if (!flag || flag.getKey().getValue() != name)
        continue;
      auto value = mlir::dyn_cast<mlir::IntegerAttr>(flag.getValue());
      auto behavior = name == "qir_minor_version"
                          ? mlir::LLVM::ModFlagBehavior::Max
                          : mlir::LLVM::ModFlagBehavior::Error;
      if (!value || !value.getType().isInteger(width) ||
          value.getValue().getZExtValue() != expected ||
          flag.getBehavior() != behavior)
        return false;
      ++count;
    }
    return count == 1;
  };
  if (!matches("qir_major_version", 32, flagtreeq::qir::majorVersion) ||
      !matches("qir_minor_version", 32, flagtreeq::qir::minorVersion) ||
      !matches("dynamic_qubit_management", 1, 0) ||
      !matches("dynamic_result_management", 1, 0))
    return module.emitError(
        "QIR export requires QIR 2.0 static-resource module flags");
  return mlir::success();
}

} // namespace

mlir::LogicalResult flagtreeq::qir::exportQIR(mlir::ModuleOp module,
                                              llvm::raw_ostream &output,
                                              bool emitBitcode) {
  if (mlir::failed(mlir::verify(module)) ||
      mlir::failed(checkLoweredModule(module)))
    return mlir::failure();
  mlir::registerBuiltinDialectTranslation(*module.getContext());
  mlir::registerLLVMDialectTranslation(*module.getContext());
  llvm::LLVMContext context;
  auto llvmModule = mlir::translateModuleToLLVMIR(module, context);
  if (!llvmModule)
    return mlir::failure();
  finalizeModuleFlags(*llvmModule);
  std::string diagnostics;
  llvm::raw_string_ostream stream(diagnostics);
  if (llvm::verifyModule(*llvmModule, &stream))
    return module.emitError("QIR LLVM verification failed: ") << diagnostics;
  if (emitBitcode)
    llvm::WriteBitcodeToFile(*llvmModule, output);
  else
    llvmModule->print(output, nullptr);
  return mlir::success();
}
