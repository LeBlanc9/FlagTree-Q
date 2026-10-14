// RUN: %flagtree_q_opt %s | %flagtree_q_opt | %FileCheck %s
// RUN: %flagtree_q_opt %s --emit-bytecode | %flagtree_q_opt | %FileCheck %s
// RUN: %flagtree_q_opt %s --mlir-print-op-generic | %flagtree_q_opt | %FileCheck %s
// RUN: %flagtree_q_opt %s --canonicalize --cse | %FileCheck %s

// CHECK-LABEL: func.func @random_bit() -> i1
func.func @random_bit() -> i1 {
  // CHECK: %[[Q:.*]] = quanta.alloc : !quanta.qubit
  %q = quanta.alloc : !quanta.qubit
  // CHECK-NEXT: quanta.h %[[Q]] : (!quanta.qubit) -> ()
  quanta.h %q : (!quanta.qubit) -> ()
  // CHECK-NEXT: %[[BIT:.*]] = quanta.measure %[[Q]] : !quanta.qubit -> i1
  %bit = quanta.measure %q : !quanta.qubit -> i1
  // Deallocation accepts a measured qubit without a preceding reset.
  // CHECK-NEXT: quanta.dealloc %[[Q]] : !quanta.qubit
  quanta.dealloc %q : !quanta.qubit
  // The classical result remains valid after releasing the qubit.
  // CHECK-NEXT: return %[[BIT]] : i1
  return %bit : i1
}

// A measurement can drive classical control flow. The same qubit can be used
// again after measurement and reset.
// CHECK-LABEL: func.func @feedback() -> i1
func.func @feedback() -> i1 {
  // CHECK: %[[Q:.*]] = quanta.alloc : !quanta.qubit
  %q = quanta.alloc : !quanta.qubit
  // CHECK-NEXT: quanta.h %[[Q]]
  quanta.h %q : (!quanta.qubit) -> ()
  // CHECK-NEXT: %[[BIT:.*]] = quanta.measure %[[Q]] : !quanta.qubit -> i1
  %bit = quanta.measure %q : !quanta.qubit -> i1
  // CHECK-NEXT: scf.if %[[BIT]] {
  scf.if %bit {
    // CHECK-NEXT: quanta.x %[[Q]] : (!quanta.qubit) -> ()
    quanta.x %q : (!quanta.qubit) -> ()
  }
  // CHECK: quanta.reset %[[Q]] : !quanta.qubit
  quanta.reset %q : !quanta.qubit
  // CHECK-NEXT: quanta.h %[[Q]]
  quanta.h %q : (!quanta.qubit) -> ()
  // CHECK-NEXT: %[[SECOND:.*]] = quanta.measure %[[Q]] : !quanta.qubit -> i1
  %second = quanta.measure %q : !quanta.qubit -> i1
  // CHECK-NEXT: quanta.dealloc %[[Q]]
  quanta.dealloc %q : !quanta.qubit
  // CHECK-NEXT: return %[[SECOND]] : i1
  return %second : i1
}

// Function parameters are borrowed; measurement and reset do not consume them.
// CHECK-LABEL: func.func @borrowed(
// CHECK-SAME: %[[Q:.*]]: !quanta.qubit) -> i1
func.func @borrowed(%q: !quanta.qubit) -> i1 {
  // CHECK: %[[BIT:.*]] = quanta.measure %[[Q]] : !quanta.qubit -> i1
  %bit = quanta.measure %q : !quanta.qubit -> i1
  // CHECK-NEXT: quanta.reset %[[Q]] : !quanta.qubit
  quanta.reset %q : !quanta.qubit
  // CHECK-NEXT: return %[[BIT]] : i1
  return %bit : i1
}

// Discard may operate on an unmeasured, entangled qubit in any state.
// CHECK-LABEL: func.func @discard_entangled() -> i1
func.func @discard_entangled() -> i1 {
  // CHECK: %[[A:.*]] = quanta.alloc : !quanta.qubit
  // CHECK-NEXT: %[[B:.*]] = quanta.alloc : !quanta.qubit
  %a = quanta.alloc : !quanta.qubit
  %b = quanta.alloc : !quanta.qubit
  // CHECK-NEXT: quanta.h %[[A]]
  quanta.h %a : (!quanta.qubit) -> ()
  // CHECK-NEXT: quanta.x [%[[A]]] %[[B]]
  quanta.x [%a] %b : (!quanta.qubit, !quanta.qubit) -> ()
  // CHECK-NEXT: quanta.dealloc %[[A]] : !quanta.qubit
  quanta.dealloc %a : !quanta.qubit
  // CHECK-NEXT: %[[BIT:.*]] = quanta.measure %[[B]] : !quanta.qubit -> i1
  %bit = quanta.measure %b : !quanta.qubit -> i1
  // CHECK-NEXT: quanta.dealloc %[[B]] : !quanta.qubit
  quanta.dealloc %b : !quanta.qubit
  // CHECK-NEXT: return %[[BIT]] : i1
  return %bit : i1
}

// Releases on mutually exclusive paths are not duplicate frees. Do not reject
// them by counting uses of an allocation, or by walking in textual order.
// CHECK-LABEL: func.func @branch_cleanup(
// CHECK-SAME: %[[COND:.*]]: i1)
func.func @branch_cleanup(%cond: i1) {
  // CHECK: %[[Q:.*]] = quanta.alloc : !quanta.qubit
  %q = quanta.alloc : !quanta.qubit
  // CHECK-NEXT: scf.if %[[COND]] {
  scf.if %cond {
    // CHECK-NEXT: quanta.x %[[Q]]
    quanta.x %q : (!quanta.qubit) -> ()
    // CHECK-NEXT: quanta.dealloc %[[Q]] : !quanta.qubit
    quanta.dealloc %q : !quanta.qubit
  // CHECK: } else {
  } else {
    // CHECK-NEXT: quanta.dealloc %[[Q]] : !quanta.qubit
    quanta.dealloc %q : !quanta.qubit
  }
  // CHECK: return
  return
}
