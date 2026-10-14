// RUN: %flagtree_q_opt %s | %flagtree_q_opt | %FileCheck %s
// RUN: %flagtree_q_opt %s --emit-bytecode | %flagtree_q_opt | %FileCheck %s

module {
  func.func private @kernel(!quanta.qubit, !quanta.qubit, f64)
  func.func private @generic_spelling(!quanta<qubit>)

  func.func @forward(%qubit: !quanta.qubit) -> !quanta.qubit {
    return %qubit : !quanta.qubit
  }

  func.func @call_kernel(%q0: !quanta.qubit, %q1: !quanta.qubit, %theta: f64) {
    func.call @kernel(%q0, %q1, %theta) : (!quanta.qubit, !quanta.qubit, f64) -> ()
    return
  }
}

// CHECK-LABEL: func.func private @kernel(!quanta.qubit, !quanta.qubit, f64)
// CHECK-LABEL: func.func private @generic_spelling(!quanta.qubit)
// CHECK-LABEL: func.func @forward(
// CHECK-SAME: %[[Q:.*]]: !quanta.qubit) -> !quanta.qubit
// CHECK: return %[[Q]] : !quanta.qubit
// CHECK-LABEL: func.func @call_kernel(
// CHECK-SAME: !quanta.qubit, {{.*}}: !quanta.qubit, {{.*}}: f64)
// CHECK: call @kernel({{.*}}) : (!quanta.qubit, !quanta.qubit, f64) -> ()
// CHECK: return
