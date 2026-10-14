#include "FlagTreeQ/Dialect/Quanta/IR/QuantaDialect.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Math/IR/Math.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"

int main(int argc, char **argv) {
  mlir::DialectRegistry registry;
  registry.insert<flagtreeq::quanta::QuantaDialect, mlir::arith::ArithDialect,
                  mlir::func::FuncDialect, mlir::math::MathDialect,
                  mlir::scf::SCFDialect>();
  return mlir::asMainReturnCode(mlir::MlirOptMain(
      argc, argv, "FlagTree-Q modular optimizer driver\n", registry));
}
