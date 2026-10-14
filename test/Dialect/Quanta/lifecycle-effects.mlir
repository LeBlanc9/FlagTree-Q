// RUN: %flagtree_q_opt %s --canonicalize --cse | %FileCheck %s

// Distinct allocations must not be CSE'd even though neither has any operands.
// Unused measurements and resets must not be removed or reordered by generic
// optimizations; they update state observable through the borrowed reference.
// CHECK-LABEL: func.func @effects(
// CHECK-SAME: %[[Q:.*]]: !quanta.qubit)
func.func @effects(%q: !quanta.qubit) {
  // CHECK: %[[A:.*]] = quanta.alloc : !quanta.qubit
  // CHECK-NEXT: %[[B:.*]] = quanta.alloc : !quanta.qubit
  %a = quanta.alloc : !quanta.qubit
  %b = quanta.alloc : !quanta.qubit
  // CHECK-NEXT: quanta.h %[[Q]]
  quanta.h %q : (!quanta.qubit) -> ()
  // CHECK-NEXT: %{{.*}} = quanta.measure %[[Q]] : !quanta.qubit -> i1
  %unused0 = quanta.measure %q : !quanta.qubit -> i1
  // CHECK-NEXT: %{{.*}} = quanta.measure %[[Q]] : !quanta.qubit -> i1
  %unused1 = quanta.measure %q : !quanta.qubit -> i1
  // CHECK-NEXT: quanta.h %[[Q]]
  quanta.h %q : (!quanta.qubit) -> ()
  // CHECK-NEXT: quanta.reset %[[Q]] : !quanta.qubit
  quanta.reset %q : !quanta.qubit
  // CHECK-NEXT: quanta.reset %[[Q]] : !quanta.qubit
  quanta.reset %q : !quanta.qubit
  // CHECK-NEXT: quanta.dealloc %[[A]] : !quanta.qubit
  quanta.dealloc %a : !quanta.qubit
  // CHECK-NEXT: quanta.dealloc %[[B]] : !quanta.qubit
  quanta.dealloc %b : !quanta.qubit
  // CHECK-NEXT: return
  return
}

// Side effects inside classical regions must survive even with no SSA results.
// CHECK-LABEL: func.func @region_effects(
// CHECK-SAME: %[[Q:.*]]: !quanta.qubit, %[[COND:.*]]: i1)
func.func @region_effects(%q: !quanta.qubit, %cond: i1) {
  // CHECK: scf.if %[[COND]] {
  scf.if %cond {
    // CHECK-NEXT: %{{.*}} = quanta.measure %[[Q]] : !quanta.qubit -> i1
    %unused = quanta.measure %q : !quanta.qubit -> i1
    // CHECK-NEXT: quanta.reset %[[Q]] : !quanta.qubit
    quanta.reset %q : !quanta.qubit
  }
  // CHECK: return
  return
}
