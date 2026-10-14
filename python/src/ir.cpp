#include "FlagTreeQ/Dialect/Quanta/IR/QuantaDialect.h"
#include "FlagTreeQ/Dialect/Quanta/IR/QuantaOps.h"
#include "FlagTreeQ/Dialect/Quanta/IR/QuantaTypes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace py = pybind11;
namespace quanta = flagtreeq::quanta;

namespace {

// Handles retain their owning module/context. No erasure or mutating passes are
// exposed: those would require handle invalidation beyond shared ownership.
struct Context {
  mlir::MLIRContext context;

  Context() {
    context.loadDialect<quanta::QuantaDialect, mlir::arith::ArithDialect,
                        mlir::func::FuncDialect>();
  }
};

struct Type {
  std::shared_ptr<Context> owner;
  mlir::Type type;
};

struct Module {
  std::shared_ptr<Context> owner;
  mlir::OwningOpRef<mlir::ModuleOp> operation;

  explicit Module(std::shared_ptr<Context> owner)
      : owner(std::move(owner)),
        operation(mlir::ModuleOp::create(
            mlir::UnknownLoc::get(&this->owner->context))) {}

  std::string str(bool debugInfo = false) {
    std::string text;
    llvm::raw_string_ostream stream(text);
    mlir::OpPrintingFlags flags;
    flags.enableDebugInfo(debugInfo);
    operation->print(stream, flags);
    return text;
  }
};

struct Value {
  std::shared_ptr<Module> owner;
  mlir::Value value;
};

struct Function {
  std::shared_ptr<Module> owner;
  mlir::func::FuncOp operation;

  std::vector<Value> arguments() {
    std::vector<Value> result;
    for (auto arg : operation.getArguments())
      result.push_back({owner, arg});
    return result;
  }
};

// Preserve native verifier diagnostics instead of letting them disappear on
// stderr. The same verifier is used by the standalone optimizer driver.
std::string verificationError(mlir::Operation *operation) {
  std::string text;
  llvm::raw_string_ostream stream(text);
  mlir::ScopedDiagnosticHandler handler(
      operation->getContext(), [&](mlir::Diagnostic &diagnostic) {
        if (auto loc =
                mlir::dyn_cast<mlir::FileLineColLoc>(diagnostic.getLocation()))
          stream << loc.getFilename().getValue() << ':' << loc.getLine() << ':'
                 << loc.getColumn();
        else
          diagnostic.getLocation().print(stream);
        stream << ": ";
        diagnostic.print(stream);
        stream << '\n';
        for (auto &note : diagnostic.getNotes()) {
          note.print(stream);
          stream << '\n';
        }
        return mlir::success();
      });
  if (mlir::succeeded(mlir::verify(operation)))
    return {};
  return text.empty() ? "MLIR verification failed" : text;
}

struct Builder {
  std::shared_ptr<Module> owner;
  mlir::OpBuilder builder;
  mlir::Location location;

  explicit Builder(std::shared_ptr<Module> owner)
      : owner(std::move(owner)), builder(&this->owner->owner->context),
        location(builder.getUnknownLoc()) {}

  void checkInsertionPoint() {
    auto *block = builder.getInsertionBlock();
    if (!block)
      throw std::invalid_argument(
          "create a function before inserting operations");
    if (!block->empty() &&
        block->back().hasTrait<mlir::OpTrait::IsTerminator>())
      throw std::invalid_argument(
          "cannot insert operations after a terminator");
  }

  mlir::Value unwrap(const Value &value) {
    if (value.owner != owner)
      throw std::invalid_argument("operand belongs to a different module");
    auto operand = value.value;
    if (operand.getParentBlock() != builder.getInsertionBlock())
      throw std::invalid_argument("operand belongs to a different function");
    return operand;
  }

  llvm::SmallVector<mlir::Value> unwrap(const std::vector<Value> &values) {
    llvm::SmallVector<mlir::Value> result;
    for (const auto &value : values)
      result.push_back(unwrap(value));
    return result;
  }

  llvm::SmallVector<mlir::Type> unwrap(const std::vector<Type> &types) {
    llvm::SmallVector<mlir::Type> result;
    for (const auto &type : types) {
      if (type.owner != owner->owner)
        throw std::invalid_argument("type belongs to a different context");
      result.push_back(type.type);
    }
    return result;
  }

  void checkOperation(mlir::Operation *operation) {
    auto error = verificationError(operation);
    if (!error.empty()) {
      // Nothing referencing this operation has escaped to Python yet.
      operation->erase();
      throw std::invalid_argument(error);
    }
  }

  Function createFunction(const std::string &name,
                          const std::vector<Type> &inputs,
                          const std::vector<Type> &results) {
    if (name.empty())
      throw std::invalid_argument("function name must not be empty");
    if (mlir::SymbolTable::lookupSymbolIn(*owner->operation, name))
      throw std::invalid_argument("function already exists: " + name);
    auto inputTypes = unwrap(inputs);
    auto resultTypes = unwrap(results);
    auto type = builder.getFunctionType(inputTypes, resultTypes);
    auto function = mlir::func::FuncOp::create(location, name, type);
    owner->operation->push_back(function);
    builder.setInsertionPointToEnd(function.addEntryBlock());
    return {owner, function};
  }

  template <typename Op, typename... Args> Value createValue(Args &&...args) {
    checkInsertionPoint();
    auto op = Op::create(builder, location, std::forward<Args>(args)...);
    checkOperation(op);
    return {owner, op->getResult(0)};
  }

  template <typename Op> void createUnary(const Value &value) {
    checkInsertionPoint();
    auto operand = unwrap(value);
    auto op = Op::create(builder, location, operand);
    checkOperation(op);
  }

  template <typename Op>
  Value createBinary(const Value &lhs, const Value &rhs, mlir::Type type) {
    checkInsertionPoint();
    auto left = unwrap(lhs);
    auto right = unwrap(rhs);
    if (left.getType() != type || right.getType() != type)
      throw std::invalid_argument("arithmetic operand types do not match");
    return createValue<Op>(left, right);
  }

  template <typename Op>
  void createGate(const std::vector<Value> &parameters,
                  const std::vector<Value> &controls,
                  const std::vector<Value> &targets, bool adj) {
    checkInsertionPoint();
    auto args = unwrap(parameters);
    auto ctrls = unwrap(controls);
    auto tgts = unwrap(targets);
    auto op = Op::create(builder, location, args, ctrls, tgts, adj);
    checkOperation(op);
  }

  void createReturn(const std::vector<Value> &values) {
    checkInsertionPoint();
    auto operands = unwrap(values);
    auto function = mlir::cast<mlir::func::FuncOp>(
        builder.getInsertionBlock()->getParentOp());
    auto results = function.getFunctionType().getResults();
    if (results.size() != operands.size())
      throw std::invalid_argument(
          "return count does not match function signature");
    for (auto [type, operand] : llvm::zip(results, operands))
      if (type != operand.getType())
        throw std::invalid_argument(
            "return types do not match function signature");
    auto op = mlir::func::ReturnOp::create(builder, location, operands);
    checkOperation(op);
  }
};

template <typename Op>
void bindSingleTargetGate(py::class_<Builder> &binding, const char *name) {
  binding.def(
      name,
      [](Builder &builder, const Value &target,
         const std::vector<Value> &controls,
         bool adj) { builder.createGate<Op>({}, controls, {target}, adj); },
      py::arg("target"), py::kw_only(),
      py::arg("controls") = std::vector<Value>{}, py::arg("adj") = false);
}

template <typename Op>
void bindRotationGate(py::class_<Builder> &binding, const char *name) {
  binding.def(
      name,
      [](Builder &builder, const Value &theta, const Value &target,
         const std::vector<Value> &controls, bool adj) {
        builder.createGate<Op>({theta}, controls, {target}, adj);
      },
      py::arg("theta"), py::arg("target"), py::kw_only(),
      py::arg("controls") = std::vector<Value>{}, py::arg("adj") = false);
}

template <typename Op>
void bindBinary(py::class_<Builder> &binding, const char *name, bool floating) {
  binding.def(
      name, [floating](Builder &builder, const Value &lhs, const Value &rhs) {
        mlir::Type type = floating ? mlir::Type(builder.builder.getF64Type())
                                   : mlir::Type(builder.builder.getI64Type());
        return builder.createBinary<Op>(lhs, rhs, type);
      });
}

} // namespace

void initIR(py::module_ &module) {
  module.doc() = "Internal MLIR builder API; not the public quantum language";
  py::class_<Context, std::shared_ptr<Context>>(module, "Context")
      .def(py::init<>())
      .def_property_readonly(
          "qubit",
          [](std::shared_ptr<Context> context) {
            return Type{context, quanta::QubitType::get(&context->context)};
          })
      .def_property_readonly(
          "f64",
          [](std::shared_ptr<Context> context) {
            return Type{context, mlir::Float64Type::get(&context->context)};
          })
      .def_property_readonly(
          "i1",
          [](std::shared_ptr<Context> context) {
            return Type{context, mlir::IntegerType::get(&context->context, 1)};
          })
      .def_property_readonly("i64", [](std::shared_ptr<Context> context) {
        return Type{context, mlir::IntegerType::get(&context->context, 64)};
      });

  py::class_<Type>(module, "Type")
      .def("__str__",
           [](const Type &type) {
             std::string text;
             llvm::raw_string_ostream stream(text);
             type.type.print(stream);
             return text;
           })
      .def(
          "__eq__",
          [](const Type &lhs, const Type &rhs) {
            return lhs.owner == rhs.owner && lhs.type == rhs.type;
          },
          py::is_operator());

  py::class_<Module, std::shared_ptr<Module>>(module, "Module")
      .def(py::init<std::shared_ptr<Context>>(), py::arg("context").none(false))
      .def("__str__", [](Module &module) { return module.str(); })
      .def("to_string", &Module::str, py::arg("debug_info") = false)
      .def("verify", [](Module &module) {
        auto error = verificationError(*module.operation);
        if (!error.empty())
          throw std::invalid_argument(error);
        return true;
      });

  py::class_<Value>(module, "Value")
      .def_property_readonly("type", [](const Value &value) {
        return Type{value.owner->owner, value.value.getType()};
      });

  py::class_<Function>(module, "Function")
      .def_property_readonly("arguments", &Function::arguments)
      .def_property_readonly("name", [](Function &function) {
        return function.operation.getSymName().str();
      });

  auto builder = py::class_<Builder>(module, "Builder")
                     .def(py::init<std::shared_ptr<Module>>(),
                          py::arg("module").none(false));
  builder
      .def(
          "set_location",
          [](Builder &builder, const std::string &filename, unsigned line,
             unsigned column) {
            if (line == 0 || column == 0)
              throw std::invalid_argument(
                  "source line and column must be positive");
            builder.location = mlir::FileLineColLoc::get(
                builder.builder.getContext(), filename, line, column);
          },
          py::arg("filename"), py::arg("line"), py::arg("column"))
      .def("create_function", &Builder::createFunction, py::arg("name"),
           py::arg("inputs") = std::vector<Type>{},
           py::arg("results") = std::vector<Type>{})
      .def("set_insertion_point",
           [](Builder &builder, Function &function) {
             if (function.owner != builder.owner)
               throw std::invalid_argument(
                   "function belongs to a different module");
             builder.builder.setInsertionPointToEnd(
                 &function.operation.front());
           })
      .def("create_f64",
           [](Builder &builder, double value) {
             return builder.createValue<mlir::arith::ConstantOp>(
                 builder.builder.getF64FloatAttr(value));
           })
      .def(
          "create_i1",
          [](Builder &builder, bool value) {
            return builder.createValue<mlir::arith::ConstantOp>(
                builder.builder.getBoolAttr(value));
          },
          py::arg("value").noconvert())
      .def(
          "create_i64",
          [](Builder &builder, int64_t value) {
            return builder.createValue<mlir::arith::ConstantOp>(
                builder.builder.getI64IntegerAttr(value));
          },
          py::arg("value").noconvert())
      .def("create_alloc",
           [](Builder &builder) {
             return builder.createValue<quanta::AllocOp>(
                 quanta::QubitType::get(builder.builder.getContext()));
           })
      .def("create_measure",
           [](Builder &builder, const Value &qubit) {
             builder.checkInsertionPoint();
             return builder.createValue<quanta::MeasureOp>(
                 builder.builder.getI1Type(), builder.unwrap(qubit));
           })
      .def("create_reset", &Builder::createUnary<quanta::ResetOp>)
      .def("create_dealloc", &Builder::createUnary<quanta::DeallocOp>)
      .def("create_return", &Builder::createReturn,
           py::arg("values") = std::vector<Value>{});

  bindBinary<mlir::arith::AddFOp>(builder, "create_addf", true);
  bindBinary<mlir::arith::SubFOp>(builder, "create_subf", true);
  bindBinary<mlir::arith::MulFOp>(builder, "create_mulf", true);
  bindBinary<mlir::arith::DivFOp>(builder, "create_divf", true);
  bindBinary<mlir::arith::AddIOp>(builder, "create_addi", false);
  bindBinary<mlir::arith::SubIOp>(builder, "create_subi", false);
  bindBinary<mlir::arith::MulIOp>(builder, "create_muli", false);
  builder.def("create_negf", [](Builder &builder, const Value &value) {
    builder.checkInsertionPoint();
    auto operand = builder.unwrap(value);
    if (!operand.getType().isF64())
      throw std::invalid_argument("negf requires an f64 operand");
    return builder.createValue<mlir::arith::NegFOp>(operand);
  });
  builder.def("create_sitofp", [](Builder &builder, const Value &value) {
    builder.checkInsertionPoint();
    auto operand = builder.unwrap(value);
    if (!operand.getType().isInteger(64))
      throw std::invalid_argument("sitofp requires an i64 operand");
    return builder.createValue<mlir::arith::SIToFPOp>(
        builder.builder.getF64Type(), operand);
  });

  bindSingleTargetGate<quanta::HOp>(builder, "create_h");
  bindSingleTargetGate<quanta::XOp>(builder, "create_x");
  bindSingleTargetGate<quanta::YOp>(builder, "create_y");
  bindSingleTargetGate<quanta::ZOp>(builder, "create_z");
  bindSingleTargetGate<quanta::SOp>(builder, "create_s");
  bindSingleTargetGate<quanta::TOp>(builder, "create_t");
  bindRotationGate<quanta::RXOp>(builder, "create_rx");
  bindRotationGate<quanta::RYOp>(builder, "create_ry");
  bindRotationGate<quanta::RZOp>(builder, "create_rz");
  builder.def(
      "create_swap",
      [](Builder &builder, const Value &first, const Value &second,
         const std::vector<Value> &controls, bool adj) {
        builder.createGate<quanta::SwapOp>({}, controls, {first, second}, adj);
      },
      py::arg("first"), py::arg("second"), py::kw_only(),
      py::arg("controls") = std::vector<Value>{}, py::arg("adj") = false);
}
