#include "FlagTreeQ/Runtime/QIR/Executor.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include <cmath>
#include <mutex>

using namespace llvm;
using flagtreeq::qir::Function;
using flagtreeq::runtime::QIRCallbacks;
using flagtreeq::runtime::QIRExecutable;

namespace {
Error invalid(const Twine &message) {
  return createStringError(inconvertibleErrorCode(),
                           "unsupported FlagTree-Q QIR: " + message);
}

thread_local QIRCallbacks *activeCallbacks = nullptr;

uint64_t id(void *handle) { return reinterpret_cast<uintptr_t>(handle); }

template <Function F> void gate1(void *q) noexcept {
  activeCallbacks->gate(F, {id(q)}, 0.0);
}
template <Function F> void rotation(double angle, void *q) noexcept {
  activeCallbacks->gate(F, {id(q)}, angle);
}
template <Function F> void gate2(void *a, void *b) noexcept {
  activeCallbacks->gate(F, {id(a), id(b)}, 0.0);
}
void ccx(void *a, void *b, void *c) noexcept {
  activeCallbacks->gate(Function::CCX, {id(a), id(b), id(c)}, 0.0);
}
void initialize(void *) noexcept { activeCallbacks->initialize(); }
void measure(void *q, void *r) noexcept {
  activeCallbacks->measure(id(q), id(r));
}
void recordResult(void *r, const char *) noexcept {
  activeCallbacks->recordResult(id(r));
}
void recordTuple(uint64_t size, const char *) noexcept {
  activeCallbacks->recordTuple(size);
}

struct Symbol {
  Function function;
  orc::ExecutorAddr address;
  unsigned qubits;
  bool rotation = false;
};

const Symbol symbols[] = {
    {Function::H, orc::ExecutorAddr::fromPtr(&gate1<Function::H>), 1},
    {Function::X, orc::ExecutorAddr::fromPtr(&gate1<Function::X>), 1},
    {Function::Y, orc::ExecutorAddr::fromPtr(&gate1<Function::Y>), 1},
    {Function::Z, orc::ExecutorAddr::fromPtr(&gate1<Function::Z>), 1},
    {Function::S, orc::ExecutorAddr::fromPtr(&gate1<Function::S>), 1},
    {Function::SAdj, orc::ExecutorAddr::fromPtr(&gate1<Function::SAdj>), 1},
    {Function::T, orc::ExecutorAddr::fromPtr(&gate1<Function::T>), 1},
    {Function::TAdj, orc::ExecutorAddr::fromPtr(&gate1<Function::TAdj>), 1},
    {Function::RX, orc::ExecutorAddr::fromPtr(&rotation<Function::RX>), 1, true},
    {Function::RY, orc::ExecutorAddr::fromPtr(&rotation<Function::RY>), 1, true},
    {Function::RZ, orc::ExecutorAddr::fromPtr(&rotation<Function::RZ>), 1, true},
    {Function::Swap, orc::ExecutorAddr::fromPtr(&gate2<Function::Swap>), 2},
    {Function::CNOT, orc::ExecutorAddr::fromPtr(&gate2<Function::CNOT>), 2},
    {Function::CZ, orc::ExecutorAddr::fromPtr(&gate2<Function::CZ>), 2},
    {Function::CCX, orc::ExecutorAddr::fromPtr(&ccx), 3},
    {Function::MZ, orc::ExecutorAddr::fromPtr(&measure), 0},
    {Function::Initialize, orc::ExecutorAddr::fromPtr(&initialize), 0},
    {Function::RecordResult, orc::ExecutorAddr::fromPtr(&recordResult), 0},
    {Function::RecordTuple, orc::ExecutorAddr::fromPtr(&recordTuple), 0},
};

const Symbol *findSymbol(StringRef name) {
  for (const auto &symbol : symbols)
    if (flagtreeq::qir::getFunctionName(symbol.function) == name)
      return &symbol;
  return nullptr;
}

FunctionType *signature(const Symbol &symbol, LLVMContext &context) {
  auto *ptr = PointerType::getUnqual(context);
  SmallVector<Type *> args;
  if (symbol.qubits) {
    if (symbol.rotation)
      args.push_back(Type::getDoubleTy(context));
    args.append(symbol.qubits, ptr);
  } else if (symbol.function == Function::Initialize) {
    args.push_back(ptr);
  } else if (symbol.function == Function::RecordTuple) {
    args = {Type::getInt64Ty(context), ptr};
  } else {
    args = {ptr, ptr};
  }
  return FunctionType::get(Type::getVoidTy(context), args, false);
}

Expected<uint64_t> staticID(Value *value, uint64_t bound) {
  uint64_t index = 0;
  if (!isa<ConstantPointerNull>(value)) {
    auto *expr = dyn_cast<ConstantExpr>(value);
    if (!expr || expr->getOpcode() != Instruction::IntToPtr)
      return invalid("expected a static qubit/result handle");
    auto *number = dyn_cast<ConstantInt>(expr->getOperand(0));
    if (!number || number->getBitWidth() != 64)
      return invalid("expected an i64 static handle");
    index = number->getZExtValue();
  }
  if (index >= bound)
    return invalid("qubit/result handle exceeds entry-point resources");
  return index;
}

Expected<uint64_t> resourceCount(llvm::Function &entry, StringRef name) {
  uint64_t count;
  auto attr = entry.getFnAttribute(name);
  if (!attr.isStringAttribute() || attr.getValueAsString().getAsInteger(10, count))
    return invalid("missing or invalid " + name);
  return count;
}

bool hasFlag(Module &module, StringRef name, unsigned width, uint64_t value) {
  auto *constant = mdconst::dyn_extract_or_null<ConstantInt>(module.getModuleFlag(name));
  return constant && constant->getBitWidth() == width &&
         constant->getZExtValue() == value;
}

// Validate the executable subset before JIT compilation. In particular, all
// resource IDs, angles and output labels are constants; no user-provided memory
// accesses or arbitrary external calls are interpreted as quantum operations.
Error validate(Module &module, llvm::Function *&entry, uint64_t &qubits,
               uint64_t &results) {
  if (!hasFlag(module, "qir_major_version", 32, 2) ||
      !hasFlag(module, "qir_minor_version", 32, 0) ||
      !hasFlag(module, "dynamic_qubit_management", 1, 0) ||
      !hasFlag(module, "dynamic_result_management", 1, 0))
    return invalid("requires QIR 2.0 with static resources");
  if (!module.alias_empty() || !module.ifunc_empty() ||
      !module.getModuleInlineAsm().empty())
    return invalid("aliases, indirect functions and module assembly");
  for (auto &global : module.globals()) {
    auto *data = global.hasInitializer()
                     ? dyn_cast<ConstantDataArray>(global.getInitializer())
                     : nullptr;
    if (!global.hasLocalLinkage() || !global.isConstant() || !data ||
        !data->isCString())
      return invalid("only local constant output-label strings are supported");
  }
  for (auto &function : module) {
    if (function.getCallingConv() != CallingConv::C)
      return invalid("non-C calling convention");
    if (!function.isDeclaration()) {
      if (entry || !function.hasFnAttribute("entry_point"))
        return invalid("requires exactly one defined entry point");
      entry = &function;
    } else {
      auto *symbol = findSymbol(function.getName());
      if (!symbol || function.getFunctionType() != signature(*symbol, module.getContext()))
        return invalid("unknown QIR declaration or signature: " + function.getName());
    }
  }
  if (!entry || !entry->arg_empty() || entry->isVarArg() ||
      !entry->getReturnType()->isIntegerTy(64) ||
      entry->getFnAttribute("qir_profiles").getValueAsString() != "base_profile" ||
      entry->getFnAttribute("output_labeling_schema").getValueAsString() != "flagtree_q_v1")
    return invalid("requires a closed i64() base_profile entry with flagtree_q_v1 output");
  auto nQubits = resourceCount(*entry, "required_num_qubits");
  if (!nQubits)
    return nQubits.takeError();
  auto nResults = resourceCount(*entry, "required_num_results");
  if (!nResults)
    return nResults.takeError();
  qubits = *nQubits;
  results = *nResults;

  DenseSet<BasicBlock *> visited;
  DenseSet<uint64_t> measuredQubits, measuredResults;
  bool initialized = false, output = false;
  auto *block = &entry->getEntryBlock();
  while (block) {
    if (!visited.insert(block).second)
      return invalid("control-flow cycles");
    BasicBlock *next = nullptr;
    for (auto &inst : *block) {
      if (auto *branch = dyn_cast<BranchInst>(&inst)) {
        if (!branch->isUnconditional())
          return invalid("conditional control flow");
        next = branch->getSuccessor(0);
        continue;
      }
      if (auto *ret = dyn_cast<ReturnInst>(&inst)) {
        auto *value = dyn_cast<ConstantInt>(ret->getReturnValue());
        if (!value || !value->isZero())
          return invalid("entry point must return status zero");
        continue;
      }
      auto *call = dyn_cast<CallInst>(&inst);
      auto *callee = call ? call->getCalledFunction() : nullptr;
      const auto *symbol = callee ? findSymbol(callee->getName()) : nullptr;
      if (!symbol || !callee->isDeclaration() || call->hasOperandBundles() ||
          call->getCallingConv() != CallingConv::C ||
          call->getFunctionType() != signature(*symbol, module.getContext()))
        return invalid("only direct calls to the supported QIR ABI are executable");
      if (symbol->function == Function::Initialize) {
        if (initialized || !isa<ConstantPointerNull>(call->getArgOperand(0)))
          return invalid("initialize must occur once with a null argument");
        initialized = true;
        continue;
      }
      if (!initialized || output)
        return invalid("operations must follow initialize and precede output");
      if (symbol->qubits) {
        if (!measuredQubits.empty())
          return invalid("gates after measurement");
        if (symbol->rotation) {
          auto *angle = dyn_cast<ConstantFP>(call->getArgOperand(0));
          if (!angle || !std::isfinite(angle->getValueAPF().convertToDouble()))
            return invalid("rotation angles must be finite constants");
        }
        DenseSet<uint64_t> operands;
        for (unsigned i = symbol->rotation; i < call->arg_size(); ++i) {
          auto index = staticID(call->getArgOperand(i), qubits);
          if (!index)
            return index.takeError();
          if (!operands.insert(*index).second)
            return invalid("gate operands must be distinct qubits");
        }
      } else if (symbol->function == Function::MZ) {
        auto qubit = staticID(call->getArgOperand(0), qubits);
        if (!qubit)
          return qubit.takeError();
        auto result = staticID(call->getArgOperand(1), results);
        if (!result)
          return result.takeError();
        if (!measuredQubits.insert(*qubit).second ||
            !measuredResults.insert(*result).second)
          return invalid("repeated qubit measurement or result assignment");
      } else {
        StringRef label;
        if (!getConstantStringInfo(call->getArgOperand(1), label))
          return invalid("output labels must be constant strings");
        if (symbol->function == Function::RecordResult) {
          auto result = staticID(call->getArgOperand(0), results);
          if (!result)
            return result.takeError();
          if (!measuredResults.contains(*result) || label != "result")
            return invalid("output must record a measured result with label 'result'");
        } else {
          auto *size = dyn_cast<ConstantInt>(call->getArgOperand(0));
          if (!size || !size->isZero() || label != "unit")
            return invalid("void output must record an empty tuple labeled 'unit'");
        }
        output = true;
      }
    }
    block = next;
  }
  if (visited.size() != entry->size() || !initialized || !output ||
      measuredResults.size() != results)
    return invalid("incomplete execution path, resource counts or output");
  return Error::success();
}
} // namespace

struct QIRExecutable::Impl {
  std::unique_ptr<orc::LLJIT> jit;
  int64_t (*entry)();
};

QIRExecutable::QIRExecutable(std::unique_ptr<Impl> impl, uint64_t qubits,
                             uint64_t results)
    : impl(std::move(impl)), numQubits(qubits), numResults(results) {}
QIRExecutable::~QIRExecutable() = default;

Expected<std::unique_ptr<QIRExecutable>> QIRExecutable::create(StringRef ir) {
  static std::once_flag initialized;
  static bool targetUnavailable = false;
  std::call_once(initialized, [] {
    targetUnavailable = InitializeNativeTarget() || InitializeNativeTargetAsmPrinter();
  });
  if (targetUnavailable)
    return invalid("LLVM native target is unavailable");
  auto context = std::make_unique<LLVMContext>();
  SMDiagnostic diagnostic;
  auto module = parseAssemblyString(ir, diagnostic, *context);
  if (!module) {
    std::string message;
    raw_string_ostream stream(message);
    diagnostic.print("QIR", stream);
    return invalid(message);
  }
  std::string errors;
  raw_string_ostream stream(errors);
  if (verifyModule(*module, &stream))
    return invalid(errors);
  llvm::Function *entry = nullptr;
  uint64_t qubits = 0, results = 0;
  if (auto error = validate(*module, entry, qubits, results))
    return error;
  auto name = entry->getName().str();
  auto jit = orc::LLJITBuilder().create();
  if (!jit)
    return jit.takeError();
  // QIR is exported without a native target. Execute it on this process's host.
  module->setTargetTriple((*jit)->getTargetTriple());
  module->setDataLayout((*jit)->getDataLayout());
  orc::SymbolMap mappings;
  for (auto &symbol : symbols)
    mappings[(*jit)->mangleAndIntern(flagtreeq::qir::getFunctionName(symbol.function))] =
        {symbol.address, JITSymbolFlags::Exported | JITSymbolFlags::Callable};
  if (auto error = (*jit)->getMainJITDylib().define(orc::absoluteSymbols(mappings)))
    return error;
  if (auto error = (*jit)->addIRModule(orc::ThreadSafeModule(std::move(module), std::move(context))))
    return error;
  auto address = (*jit)->lookup(name);
  if (!address)
    return address.takeError();
  auto impl = std::make_unique<Impl>();
  impl->entry = address->toPtr<int64_t (*)()>();
  impl->jit = std::move(*jit);
  return std::unique_ptr<QIRExecutable>(new QIRExecutable(std::move(impl), qubits, results));
}

Error QIRExecutable::run(QIRCallbacks &callbacks) {
  if (activeCallbacks)
    return invalid("nested QIR execution is not supported");
  activeCallbacks = &callbacks;
  auto status = impl->entry();
  activeCallbacks = nullptr;
  if (status != 0)
    return invalid("entry point returned a nonzero status");
  return Error::success();
}
