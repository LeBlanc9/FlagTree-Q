// RUN: %flagtree_q_opt %s | %flagtree_q_opt | %FileCheck %s
// RUN: %flagtree_q_opt %s --emit-bytecode | %flagtree_q_opt | %FileCheck %s
// RUN: %flagtree_q_opt %s --mlir-print-op-generic | %flagtree_q_opt | %FileCheck %s
// RUN: %flagtree_q_opt %s --canonicalize --cse | %FileCheck %s

// Gates have no results but must survive DCE and CSE, text and bytecode round trips.

// CHECK-LABEL: func.func @base_gates(
// CHECK-SAME: %[[A:.*]]: !quanta.qubit, %[[B:.*]]: !quanta.qubit, %[[THETA:.*]]: f64)
func.func @base_gates(%a: !quanta.qubit, %b: !quanta.qubit, %theta: f64) {
  // CHECK: quanta.h %[[A]] : (!quanta.qubit) -> ()
  // CHECK-NEXT: quanta.h %[[A]] : (!quanta.qubit) -> ()
  quanta.h %a : (!quanta.qubit) -> ()
  quanta.h %a : (!quanta.qubit) -> ()
  // CHECK-NEXT: quanta.x %[[A]] : (!quanta.qubit) -> ()
  quanta.x %a : (!quanta.qubit) -> ()
  // CHECK-NEXT: quanta.y %[[A]] : (!quanta.qubit) -> ()
  quanta.y %a : (!quanta.qubit) -> ()
  // CHECK-NEXT: quanta.z %[[A]] : (!quanta.qubit) -> ()
  quanta.z %a : (!quanta.qubit) -> ()
  // CHECK-NEXT: quanta.s %[[A]] : (!quanta.qubit) -> ()
  quanta.s %a : (!quanta.qubit) -> ()
  // CHECK-NEXT: quanta.t %[[A]] : (!quanta.qubit) -> ()
  quanta.t %a : (!quanta.qubit) -> ()
  // CHECK-NEXT: quanta.rx (%[[THETA]]) %[[A]] : (f64, !quanta.qubit) -> ()
  quanta.rx (%theta) %a : (f64, !quanta.qubit) -> ()
  // CHECK-NEXT: quanta.ry (%[[THETA]]) %[[A]] : (f64, !quanta.qubit) -> ()
  quanta.ry (%theta) %a : (f64, !quanta.qubit) -> ()
  // CHECK-NEXT: quanta.rz (%[[THETA]]) %[[A]] : (f64, !quanta.qubit) -> ()
  quanta.rz (%theta) %a : (f64, !quanta.qubit) -> ()
  // CHECK-NEXT: quanta.swap %[[A]], %[[B]] : (!quanta.qubit, !quanta.qubit) -> ()
  quanta.swap %a, %b : (!quanta.qubit, !quanta.qubit) -> ()
  // CHECK-NEXT: return
  return
}

// CHECK-LABEL: func.func @modifiers(
// CHECK-SAME: %[[A:.*]]: !quanta.qubit, %[[B:.*]]: !quanta.qubit, %[[C:.*]]: !quanta.qubit, %[[THETA:.*]]: f64)
func.func @modifiers(%a: !quanta.qubit, %b: !quanta.qubit, %c: !quanta.qubit, %theta: f64) {
  // CHECK: quanta.h<adj> [%[[A]]] %[[B]]
  quanta.h<adj> [%a] %b : (!quanta.qubit, !quanta.qubit) -> ()
  // CX and CCX reuse the X op with one and two controls.
  // CHECK-NEXT: quanta.x [%[[A]]] %[[B]]
  quanta.x [%a] %b : (!quanta.qubit, !quanta.qubit) -> ()
  // CHECK-NEXT: quanta.x<adj> [%[[A]], %[[B]]] %[[C]]
  quanta.x<adj> [%a, %b] %c : (!quanta.qubit, !quanta.qubit, !quanta.qubit) -> ()
  // CHECK-NEXT: quanta.y<adj> [%[[A]]] %[[B]]
  quanta.y<adj> [%a] %b : (!quanta.qubit, !quanta.qubit) -> ()
  // CHECK-NEXT: quanta.z<adj> [%[[A]]] %[[B]]
  quanta.z<adj> [%a] %b : (!quanta.qubit, !quanta.qubit) -> ()
  // CHECK-NEXT: quanta.s<adj> %[[A]]
  quanta.s<adj> %a : (!quanta.qubit) -> ()
  // CHECK-NEXT: quanta.t<adj> %[[A]]
  quanta.t<adj> %a : (!quanta.qubit) -> ()
  // CHECK-NEXT: quanta.rx<adj> (%[[THETA]]) [%[[A]]] %[[B]]
  quanta.rx<adj> (%theta) [%a] %b : (f64, !quanta.qubit, !quanta.qubit) -> ()
  // CHECK-NEXT: quanta.ry<adj> (%[[THETA]]) [%[[A]], %[[B]]] %[[C]]
  quanta.ry<adj> (%theta) [%a, %b] %c : (f64, !quanta.qubit, !quanta.qubit, !quanta.qubit) -> ()
  // CHECK-NEXT: quanta.rz<adj> (%[[THETA]]) %[[B]]
  quanta.rz<adj> (%theta) %b : (f64, !quanta.qubit) -> ()
  // CHECK-NEXT: quanta.swap<adj> [%[[A]]] %[[B]], %[[C]]
  quanta.swap<adj> [%a] %b, %c : (!quanta.qubit, !quanta.qubit, !quanta.qubit) -> ()
  // CHECK-NEXT: return
  return
}

// A negative angle and adjoint are independent; no simplification is applied.
// CHECK-LABEL: func.func @negative_angle_adjoint(
// CHECK-SAME: %[[Q:.*]]: !quanta.qubit, %[[THETA:.*]]: f64)
func.func @negative_angle_adjoint(%q: !quanta.qubit, %theta: f64) {
  // CHECK: %[[NEG:.*]] = arith.negf %[[THETA]] : f64
  %neg = arith.negf %theta : f64
  // CHECK-NEXT: quanta.rx<adj> (%[[NEG]]) %[[Q]] : (f64, !quanta.qubit) -> ()
  quanta.rx<adj> (%neg) %q : (f64, !quanta.qubit) -> ()
  // CHECK-NEXT: return
  return
}
