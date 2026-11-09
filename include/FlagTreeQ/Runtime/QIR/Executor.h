#ifndef FLAGTREEQ_RUNTIME_QIR_EXECUTOR_H
#define FLAGTREEQ_RUNTIME_QIR_EXECUTOR_H

#include "FlagTreeQ/Target/QIR/QIR.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Error.h"
#include <memory>

namespace flagtreeq::runtime {

// Callbacks must not unwind through JIT code. A binding can retain an exception
// and rethrow it after run() returns. Execution is synchronous on the caller.
class QIRCallbacks {
public:
  virtual ~QIRCallbacks() = default;
  virtual void initialize() noexcept = 0;
  virtual void gate(qir::Function function, llvm::ArrayRef<uint64_t> qubits,
                    double angle) noexcept = 0;
  virtual void measure(uint64_t qubit, uint64_t result) noexcept = 0;
  virtual void recordResult(uint64_t result) noexcept = 0;
  virtual void recordTuple(uint64_t size) noexcept = 0;
};

// Executes only FlagTree-Q's closed, static QIR 2.0 subset. This is an internal
// compiler-validation runtime, not a general-purpose QIR loader or sandbox.
class QIRExecutable {
public:
  static llvm::Expected<std::unique_ptr<QIRExecutable>> create(llvm::StringRef ir);
  ~QIRExecutable();
  llvm::Error run(QIRCallbacks &callbacks);
  uint64_t getNumQubits() const { return numQubits; }
  uint64_t getNumResults() const { return numResults; }

private:
  struct Impl;
  explicit QIRExecutable(std::unique_ptr<Impl> impl, uint64_t qubits,
                         uint64_t results);
  std::unique_ptr<Impl> impl;
  uint64_t numQubits;
  uint64_t numResults;
};

} // namespace flagtreeq::runtime
#endif
