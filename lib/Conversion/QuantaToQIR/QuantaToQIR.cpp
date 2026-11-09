#include "FlagTreeQ/Conversion/QuantaToQIR/Passes.h"
#include "FlagTreeQ/Dialect/Quanta/IR/QuantaDialect.h"
#include "FlagTreeQ/Dialect/Quanta/IR/QuantaOps.h"
#include "FlagTreeQ/Target/QIR/QIR.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Matchers.h"
#include "mlir/IR/Verifier.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Transforms/Passes.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/TypeSwitch.h"

using namespace mlir;
namespace quanta = flagtreeq::quanta;
namespace qir = flagtreeq::qir;

namespace {

struct Gate {
  Location location;
  qir::Function function;
  FloatAttr angle;
  SmallVector<uint64_t> qubits;
};

struct Measurement {
  Location location;
  uint64_t qubit;
};

// Analyze before emitting anything. Static IDs are never reused, so discarding
// a subsystem can be deferred to program completion even if it is entangled.
struct Program {
  func::FuncOp source;
  llvm::DenseMap<Value, uint64_t> qubits;
  llvm::DenseMap<Value, uint64_t> results;
  llvm::DenseSet<Value> live;
  llvm::DenseSet<Value> measured;
  SmallVector<Gate> gates;
  SmallVector<Measurement> measurements;
  std::optional<uint64_t> output;

  LogicalResult checkQubit(Operation *op, Value qubit) {
    if (!qubits.contains(qubit) || !live.contains(qubit))
      return op->emitError(
          "QIR export requires a live, locally allocated qubit");
    return success();
  }

  template <typename Op> LogicalResult addGate(Op op, qir::Function function) {
    if (!measurements.empty())
      return op.emitError(
          "QIR Base Profile requires all gates before measurements");
    auto controls = op.getControls();
    if (!controls.empty()) {
      if (isa<quanta::XOp>(op) && controls.size() == 1)
        function = qir::Function::CNOT;
      else if (isa<quanta::XOp>(op) && controls.size() == 2)
        function = qir::Function::CCX;
      else if (isa<quanta::ZOp>(op) && controls.size() == 1)
        function = qir::Function::CZ;
      else
        return op.emitError(
            "unsupported controlled gate in the QIR instruction set; "
            "supported controlled gates are CX, CCX, and CZ");
    }
    Gate gate{op.getLoc(), function, {}, {}};
    if (!op.getParameters().empty()) {
      FloatAttr angle;
      if (!matchPattern(op.getParameters().front(), m_Constant(&angle)))
        return op.emitError(
            "QIR Base Profile requires constant rotation angles");
      auto value = angle.getValue();
      if (!value.isFinite())
        return op.emitError("QIR export requires finite rotation angles");
      if (op.getIsAdj())
        value.changeSign();
      gate.angle = FloatAttr::get(angle.getType(), value);
    } else if (op.getIsAdj()) {
      if (function == qir::Function::S)
        gate.function = qir::Function::SAdj;
      else if (function == qir::Function::T)
        gate.function = qir::Function::TAdj;
      // All other supported parameterless gates are self-adjoint.
    }
    for (Value qubit : controls) {
      if (failed(checkQubit(op, qubit)))
        return failure();
      gate.qubits.push_back(qubits.lookup(qubit));
    }
    for (Value qubit : op.getTargets()) {
      if (failed(checkQubit(op, qubit)))
        return failure();
      gate.qubits.push_back(qubits.lookup(qubit));
    }
    gates.push_back(std::move(gate));
    return success();
  }

  LogicalResult analyze() {
    if (source.isExternal() || !llvm::hasSingleElement(source.getBody()))
      return source.emitError(
          "QIR export requires a single-block function definition");
    if (source.getNumArguments() != 0)
      return source.emitError(
          "QIR Base Profile entry points cannot have parameters; "
          "use constants in a closed kernel");
    if (source.getSymName().starts_with("__quantum__"))
      return source.emitError(
          "entry point name uses the reserved QIR prefix '__quantum__'");
    auto types = source.getFunctionType().getResults();
    if (types.size() > 1 || (!types.empty() && !types.front().isInteger(1)))
      return source.emitError(
          "QIR export supports only void or a single measurement result");

    for (Operation &op : source.front()) {
      if (auto alloc = dyn_cast<quanta::AllocOp>(op)) {
        qubits.try_emplace(alloc.getQubit(), qubits.size());
        live.insert(alloc.getQubit());
      } else if (auto dealloc = dyn_cast<quanta::DeallocOp>(op)) {
        if (failed(checkQubit(&op, dealloc.getQubit())))
          return failure();
        live.erase(dealloc.getQubit());
      } else if (auto measure = dyn_cast<quanta::MeasureOp>(op)) {
        auto qubit = measure.getQubit();
        if (failed(checkQubit(&op, qubit)))
          return failure();
        if (!measured.insert(qubit).second)
          return op.emitError(
              "QIR Base Profile cannot measure a qubit more than once");
        results.try_emplace(measure.getResult(), measurements.size());
        measurements.push_back({op.getLoc(), qubits.lookup(qubit)});
      } else if (auto ret = dyn_cast<func::ReturnOp>(op)) {
        if (ret.getNumOperands()) {
          auto result = results.find(ret.getOperand(0));
          if (result == results.end())
            return ret.emitError("QIR export requires the returned bit to be a "
                                 "measurement result");
          output = result->second;
        }
      } else if (isa<arith::ConstantOp>(op)) {
        // Folded constants are materialized at their QIS call sites.
      } else if (isa<quanta::ResetOp>(op)) {
        return op.emitError("QIR Base Profile export does not support reset");
      } else {
        auto status =
            llvm::TypeSwitch<Operation *, LogicalResult>(&op)
                .Case<quanta::HOp>(
                    [&](auto gate) { return addGate(gate, qir::Function::H); })
                .Case<quanta::XOp>(
                    [&](auto gate) { return addGate(gate, qir::Function::X); })
                .Case<quanta::YOp>(
                    [&](auto gate) { return addGate(gate, qir::Function::Y); })
                .Case<quanta::ZOp>(
                    [&](auto gate) { return addGate(gate, qir::Function::Z); })
                .Case<quanta::SOp>(
                    [&](auto gate) { return addGate(gate, qir::Function::S); })
                .Case<quanta::TOp>(
                    [&](auto gate) { return addGate(gate, qir::Function::T); })
                .Case<quanta::RXOp>(
                    [&](auto gate) { return addGate(gate, qir::Function::RX); })
                .Case<quanta::RYOp>(
                    [&](auto gate) { return addGate(gate, qir::Function::RY); })
                .Case<quanta::RZOp>(
                    [&](auto gate) { return addGate(gate, qir::Function::RZ); })
                .Case<quanta::SwapOp>([&](auto gate) {
                  return addGate(gate, qir::Function::Swap);
                })
                .Default([](Operation *other) {
                  return other->emitError(
                      "unsupported operation in QIR Base Profile export; "
                      "classical computations must fold to constants");
                });
        if (failed(status))
          return failure();
      }
    }
    if (!live.empty())
      return source.emitError(
          "QIR export requires every allocated qubit to be released");
    return success();
  }

  void emit(ModuleOp module) {
    OpBuilder builder(module.getContext());
    auto location = source.getLoc();
    builder.setInsertionPointToStart(module.getBody());
    auto type = LLVM::LLVMFunctionType::get(builder.getI64Type(), {});
    auto entry =
        LLVM::LLVMFuncOp::create(builder, location, source.getSymName(), type);
    qir::setEntryPointAttributes(entry, qubits.size(), measurements.size());
    Block *initialize = entry.addEntryBlock(builder);
    Block *body = builder.createBlock(&entry.getBody());
    Block *measurementsBlock = builder.createBlock(&entry.getBody());
    Block *outputBlock = builder.createBlock(&entry.getBody());
    auto call = [&](Location loc, qir::Function function,
                    ValueRange arguments) {
      auto declaration = qir::getOrCreateDeclaration(module, function);
      LLVM::CallOp::create(builder, loc, declaration, arguments);
    };
    builder.setInsertionPointToEnd(initialize);
    auto null = LLVM::ZeroOp::create(
        builder, location, LLVM::LLVMPointerType::get(module.getContext()));
    call(location, qir::Function::Initialize, {null});
    LLVM::BrOp::create(builder, location, ValueRange{}, body);
    builder.setInsertionPointToEnd(body);
    for (auto &gate : gates) {
      SmallVector<Value> arguments;
      if (gate.angle)
        arguments.push_back(LLVM::ConstantOp::create(
            builder, gate.location, builder.getF64Type(), gate.angle));
      for (auto qubit : gate.qubits)
        arguments.push_back(
            qir::createStaticHandle(builder, gate.location, qubit));
      call(gate.location, gate.function, arguments);
    }
    LLVM::BrOp::create(builder, location, ValueRange{}, measurementsBlock);
    builder.setInsertionPointToEnd(measurementsBlock);
    for (auto [index, measurement] : llvm::enumerate(measurements)) {
      auto qubit = qir::createStaticHandle(builder, measurement.location,
                                           measurement.qubit);
      auto result =
          qir::createStaticHandle(builder, measurement.location, index);
      call(measurement.location, qir::Function::MZ, {qubit, result});
    }
    LLVM::BrOp::create(builder, location, ValueRange{}, outputBlock);

    // Even void kernels record an empty tuple. Output labels are non-null,
    // null-terminated strings as required by QIR 2.0 Base Profile.
    builder.setInsertionPointToEnd(module.getBody());
    auto label = builder.getStringAttr(output ? StringRef("result\0", 7)
                                              : StringRef("unit\0", 5));
    auto labelType =
        LLVM::LLVMArrayType::get(builder.getI8Type(), label.size());
    auto global = LLVM::GlobalOp::create(
        builder, location, labelType, true, LLVM::Linkage::Internal,
        (source.getSymName() + ".output_label").str(), label);
    builder.setInsertionPointToEnd(outputBlock);
    auto labelPointer = LLVM::AddressOfOp::create(builder, location, global);
    if (output) {
      auto result = qir::createStaticHandle(builder, location, *output);
      call(location, qir::Function::RecordResult, {result, labelPointer});
    } else {
      auto size =
          LLVM::ConstantOp::create(builder, location, builder.getI64Type(), 0);
      call(location, qir::Function::RecordTuple, {size, labelPointer});
    }
    auto exitCode =
        LLVM::ConstantOp::create(builder, location, builder.getI64Type(), 0);
    LLVM::ReturnOp::create(builder, location, ValueRange{exitCode});
    qir::addModuleFlags(module);
  }
};

struct ConvertQuantaToQIRPass
    : PassWrapper<ConvertQuantaToQIRPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(ConvertQuantaToQIRPass)

  StringRef getArgument() const final { return "convert-quanta-to-qir"; }
  StringRef getDescription() const final {
    return "Lower a closed Quanta kernel to QIR 2.0 Base Profile in LLVM "
           "dialect";
  }
  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<quanta::QuantaDialect, arith::ArithDialect,
                    func::FuncDialect, LLVM::LLVMDialect>();
  }
  void runOnOperation() override {
    auto module = getOperation();
    // Fold on a clone; errors do not leave partially lowered IR behind.
    OwningOpRef<ModuleOp> source = cast<ModuleOp>(module->clone());
    PassManager folding(module.getContext());
    folding.addPass(createCanonicalizerPass());
    if (failed(folding.run(*source)))
      return signalPassFailure();
    auto &operations = source->getBody()->getOperations();
    if (!llvm::hasSingleElement(operations) ||
        !isa<func::FuncOp>(operations.front())) {
      module.emitError("QIR export requires a module containing exactly one "
                       "kernel function");
      return signalPassFailure();
    }
    Program program;
    program.source = cast<func::FuncOp>(operations.front());
    if (failed(program.analyze()))
      return signalPassFailure();
    OwningOpRef<ModuleOp> lowered = ModuleOp::create(module.getLoc());
    program.emit(*lowered);
    if (failed(verify(*lowered)))
      return signalPassFailure();
    module.getBodyRegion().takeBody(lowered->getBodyRegion());
  }
};

} // namespace

std::unique_ptr<Pass> flagtreeq::createConvertQuantaToQIRPass() {
  return std::make_unique<ConvertQuantaToQIRPass>();
}

void flagtreeq::registerQuantaToQIRPasses() {
  PassRegistration<ConvertQuantaToQIRPass>();
}
