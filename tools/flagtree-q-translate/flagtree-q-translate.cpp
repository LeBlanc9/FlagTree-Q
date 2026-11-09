#include "FlagTreeQ/Conversion/QuantaToQIR/Passes.h"
#include "FlagTreeQ/Dialect/Quanta/IR/QuantaDialect.h"
#include "FlagTreeQ/Target/QIR/Export.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/IR/Verifier.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Tools/mlir-translate/MlirTranslateMain.h"
#include "mlir/Tools/mlir-translate/Translation.h"

namespace {

mlir::LogicalResult compileAndExport(mlir::ModuleOp module,
                                     llvm::raw_ostream &output,
                                     bool bitcode = false) {
  // Keep the direct Quanta entry point while also accepting the LLVM dialect
  // output from flagtree-q-opt. The Target library only performs translation.
  if (module.getOps<mlir::func::FuncOp>().empty())
    return flagtreeq::qir::exportQIR(module, output, bitcode);
  mlir::OwningOpRef<mlir::ModuleOp> lowered =
      mlir::cast<mlir::ModuleOp>(module->clone());
  mlir::PassManager manager(module.getContext());
  manager.addPass(flagtreeq::createConvertQuantaToQIRPass());
  if (mlir::failed(mlir::verify(*lowered)) ||
      mlir::failed(manager.run(*lowered)))
    return mlir::failure();
  return flagtreeq::qir::exportQIR(*lowered, output, bitcode);
}

} // namespace

int main(int argc, char **argv) {
  auto registerDialects = [](mlir::DialectRegistry &registry) {
    registry.insert<flagtreeq::quanta::QuantaDialect, mlir::arith::ArithDialect,
                    mlir::func::FuncDialect, mlir::LLVM::LLVMDialect>();
  };
  mlir::TranslateFromMLIRRegistration text(
      "mlir-to-qir", "Export Quanta or lowered LLVM dialect to QIR LLVM IR",
      [](mlir::ModuleOp module, llvm::raw_ostream &output) {
        return compileAndExport(module, output);
      },
      registerDialects);
  mlir::TranslateFromMLIRRegistration bitcode(
      "mlir-to-qir-bitcode",
      "Export Quanta or lowered LLVM dialect to QIR bitcode",
      [](mlir::ModuleOp module, llvm::raw_ostream &output) {
        return compileAndExport(module, output, true);
      },
      registerDialects);
  return mlir::failed(
      mlir::mlirTranslateMain(argc, argv, "FlagTree-Q translation driver\n"));
}
