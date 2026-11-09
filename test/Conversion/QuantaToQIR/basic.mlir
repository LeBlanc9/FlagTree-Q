// RUN: %flagtree_q_opt --convert-quanta-to-qir %s | %FileCheck %s

func.func @bell() -> i1 {
  %a = quanta.alloc : !quanta.qubit
  %b = quanta.alloc : !quanta.qubit
  quanta.h %a : (!quanta.qubit) -> ()
  quanta.x [%a] %b : (!quanta.qubit, !quanta.qubit) -> ()
  %r = quanta.measure %b : !quanta.qubit -> i1
  quanta.dealloc %b : !quanta.qubit
  quanta.dealloc %a : !quanta.qubit
  return %r : i1
}

// CHECK: llvm.func @bell() -> i64
// CHECK-SAME: "entry_point"
// CHECK-SAME: ["qir_profiles", "base_profile"]
// CHECK-SAME: ["required_num_qubits", "2"]
// CHECK-SAME: ["required_num_results", "1"]
// CHECK: llvm.call @__quantum__rt__initialize
// CHECK: llvm.br
// CHECK: llvm.call @__quantum__qis__h__body
// CHECK: llvm.call @__quantum__qis__cnot__body
// CHECK: llvm.br
// CHECK: llvm.call @__quantum__qis__mz__body
// CHECK: llvm.br
// CHECK: llvm.call @__quantum__rt__result_record_output
// CHECK: llvm.return {{.*}} : i64
// CHECK: llvm.func @__quantum__qis__mz__body(!llvm.ptr, !llvm.ptr {llvm.writeonly})
// CHECK-SAME: "irreversible"
// CHECK: llvm.module_flags
// CHECK-SAME: <error, "qir_major_version", 2 : i32>
// CHECK-SAME: <max, "qir_minor_version", 0 : i32>
// CHECK-SAME: <error, "dynamic_qubit_management", false>
// CHECK-SAME: <error, "dynamic_result_management", false>
