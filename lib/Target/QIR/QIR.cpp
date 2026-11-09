#include "FlagTreeQ/Target/QIR/QIR.h"
#include "mlir/IR/Builders.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/ErrorHandling.h"

using namespace mlir;

namespace flagtreeq::qir {

llvm::StringRef getFunctionName(Function function) {
  switch (function) {
  case Function::H:
    return "__quantum__qis__h__body";
  case Function::X:
    return "__quantum__qis__x__body";
  case Function::Y:
    return "__quantum__qis__y__body";
  case Function::Z:
    return "__quantum__qis__z__body";
  case Function::S:
    return "__quantum__qis__s__body";
  case Function::SAdj:
    return "__quantum__qis__s__adj";
  case Function::T:
    return "__quantum__qis__t__body";
  case Function::TAdj:
    return "__quantum__qis__t__adj";
  case Function::RX:
    return "__quantum__qis__rx__body";
  case Function::RY:
    return "__quantum__qis__ry__body";
  case Function::RZ:
    return "__quantum__qis__rz__body";
  case Function::Swap:
    return "__quantum__qis__swap__body";
  case Function::CNOT:
    return "__quantum__qis__cnot__body";
  case Function::CZ:
    return "__quantum__qis__cz__body";
  case Function::CCX:
    return "__quantum__qis__ccx__body";
  case Function::MZ:
    return "__quantum__qis__mz__body";
  case Function::Initialize:
    return "__quantum__rt__initialize";
  case Function::RecordResult:
    return "__quantum__rt__result_record_output";
  case Function::RecordTuple:
    return "__quantum__rt__tuple_record_output";
  }
  llvm_unreachable("unknown QIR function");
}

LLVM::LLVMFuncOp getOrCreateDeclaration(ModuleOp module, Function function) {
  auto name = getFunctionName(function);
  if (auto declaration = module.lookupSymbol<LLVM::LLVMFuncOp>(name))
    return declaration;
  OpBuilder builder(module.getContext());
  builder.setInsertionPointToEnd(module.getBody());
  auto pointer = LLVM::LLVMPointerType::get(module.getContext());
  SmallVector<Type> arguments;
  switch (function) {
  case Function::RX:
  case Function::RY:
  case Function::RZ:
    arguments = {builder.getF64Type(), pointer};
    break;
  case Function::Swap:
  case Function::CNOT:
  case Function::CZ:
  case Function::MZ:
  case Function::RecordResult:
    arguments = {pointer, pointer};
    break;
  case Function::CCX:
    arguments = {pointer, pointer, pointer};
    break;
  case Function::RecordTuple:
    arguments = {builder.getI64Type(), pointer};
    break;
  default:
    arguments = {pointer};
    break;
  }
  auto type = LLVM::LLVMFunctionType::get(
      LLVM::LLVMVoidType::get(module.getContext()), arguments);
  auto declaration =
      LLVM::LLVMFuncOp::create(builder, module.getLoc(), name, type);
  if (function == Function::MZ) {
    declaration.setPassthroughAttr(
        builder.getArrayAttr({builder.getStringAttr("irreversible")}));
    declaration.setArgAttr(1, "llvm.writeonly", builder.getUnitAttr());
  }
  return declaration;
}

void setEntryPointAttributes(LLVM::LLVMFuncOp entry, uint64_t qubits,
                             uint64_t results) {
  Builder builder(entry.getContext());
  auto pair = [&](llvm::StringRef key, llvm::StringRef value) {
    return builder.getArrayAttr(
        {builder.getStringAttr(key), builder.getStringAttr(value)});
  };
  entry.setPassthroughAttr(builder.getArrayAttr(
      {builder.getStringAttr("entry_point"), pair("qir_profiles", baseProfile),
       pair("required_num_qubits", std::to_string(qubits)),
       pair("required_num_results", std::to_string(results)),
       pair("output_labeling_schema", outputLabelingSchema)}));
}

void addModuleFlags(ModuleOp module) {
  OpBuilder builder(module.getContext());
  builder.setInsertionPointToEnd(module.getBody());
  auto flag = [&](LLVM::ModFlagBehavior behavior, llvm::StringRef name,
                  Attribute value) {
    return LLVM::ModuleFlagAttr::get(module.getContext(), behavior,
                                     builder.getStringAttr(name), value);
  };
  LLVM::ModuleFlagsOp::create(
      builder, module.getLoc(),
      builder.getArrayAttr(
          {flag(LLVM::ModFlagBehavior::Error, "qir_major_version",
                builder.getI32IntegerAttr(majorVersion)),
           flag(LLVM::ModFlagBehavior::Max, "qir_minor_version",
                builder.getI32IntegerAttr(minorVersion)),
           flag(LLVM::ModFlagBehavior::Error, "dynamic_qubit_management",
                builder.getBoolAttr(false)),
           flag(LLVM::ModFlagBehavior::Error, "dynamic_result_management",
                builder.getBoolAttr(false))}));
}

void finalizeModuleFlags(llvm::Module &module) {
  auto *disabled = llvm::ConstantInt::getFalse(module.getContext());
  module.setModuleFlag(llvm::Module::Error, "dynamic_qubit_management",
                       disabled);
  module.setModuleFlag(llvm::Module::Error, "dynamic_result_management",
                       disabled);
}

Value createStaticHandle(OpBuilder &builder, Location location,
                         uint64_t index) {
  auto integer =
      LLVM::ConstantOp::create(builder, location, builder.getI64Type(), index);
  return LLVM::IntToPtrOp::create(
      builder, location, LLVM::LLVMPointerType::get(builder.getContext()),
      integer, LLVM::DereferenceableAttr{});
}

} // namespace flagtreeq::qir
