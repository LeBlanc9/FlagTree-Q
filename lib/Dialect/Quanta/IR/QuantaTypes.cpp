#include "FlagTreeQ/Dialect/Quanta/IR/QuantaTypes.h"

#include "FlagTreeQ/Dialect/Quanta/IR/QuantaDialect.h"
#include "mlir/IR/DialectImplementation.h"
#include "llvm/ADT/TypeSwitch.h"

using namespace mlir;
using namespace flagtreeq::quanta;

#define GET_TYPEDEF_CLASSES
#include "FlagTreeQ/Dialect/Quanta/IR/QuantaTypes.cpp.inc"

Type QubitType::parse(AsmParser &parser) {
  // The generated singleton parser would otherwise ignore a parameter list.
  auto loc = parser.getCurrentLocation();
  if (succeeded(parser.parseOptionalLess())) {
    parser.emitError(loc, "qubit type does not accept parameters");
    return {};
  }
  return get(parser.getContext());
}

void QubitType::print(AsmPrinter &) const {}

void QuantaDialect::registerTypes() {
  addTypes<
#define GET_TYPEDEF_LIST
#include "FlagTreeQ/Dialect/Quanta/IR/QuantaTypes.cpp.inc"
      >();
}
