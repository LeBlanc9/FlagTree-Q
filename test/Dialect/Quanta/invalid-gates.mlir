// RUN: %flagtree_q_opt %s --split-input-file --verify-diagnostics

func.func @parameter_on_fixed_gate(%q: !quanta.qubit, %theta: f64) {
  // expected-error @+1 {{expects 0 parameter(s), got 1}}
  quanta.h (%theta) %q : (f64, !quanta.qubit) -> ()
  return
}

// -----

func.func @missing_angle(%q: !quanta.qubit) {
  // expected-error @+1 {{expects 1 parameter(s), got 0}}
  quanta.rx %q : (!quanta.qubit) -> ()
  return
}

// -----

func.func @extra_angle(%q: !quanta.qubit, %theta: f64) {
  // expected-error @+1 {{expects 1 parameter(s), got 2}}
  quanta.ry (%theta, %theta) %q : (f64, f64, !quanta.qubit) -> ()
  return
}

// -----

func.func @missing_target() {
  // expected-error @+1 {{expects 1 target(s), got 0}}
  quanta.x : () -> ()
  return
}

// -----

func.func @extra_target(%a: !quanta.qubit, %b: !quanta.qubit) {
  // expected-error @+1 {{expects 1 target(s), got 2}}
  quanta.z %a, %b : (!quanta.qubit, !quanta.qubit) -> ()
  return
}

// -----

func.func @swap_needs_two_targets(%a: !quanta.qubit) {
  // expected-error @+1 {{expects 2 target(s), got 1}}
  quanta.swap %a : (!quanta.qubit) -> ()
  return
}

// -----

func.func @control_is_target(%a: !quanta.qubit) {
  // expected-error @+1 {{target qubits must be distinct from each other and from controls}}
  quanta.x [%a] %a : (!quanta.qubit, !quanta.qubit) -> ()
  return
}

// -----

func.func @repeated_control(%a: !quanta.qubit, %b: !quanta.qubit) {
  // expected-error @+1 {{control qubits must be distinct}}
  quanta.x [%a, %a] %b : (!quanta.qubit, !quanta.qubit, !quanta.qubit) -> ()
  return
}

// -----

func.func @repeated_target(%a: !quanta.qubit) {
  // expected-error @+1 {{target qubits must be distinct from each other and from controls}}
  quanta.swap %a, %a : (!quanta.qubit, !quanta.qubit) -> ()
  return
}

// -----

func.func @angle_type(%q: !quanta.qubit, %theta: f32) {
  // expected-error @+1 {{operand #0 must be variadic of 64-bit float}}
  "quanta.rx"(%theta, %q) <{operandSegmentSizes = array<i32: 1, 0, 1>}> : (f32, !quanta.qubit) -> ()
  return
}

// -----

func.func @classical_control(%c: i1, %q: !quanta.qubit) {
  // expected-error @+1 {{operand #0 must be variadic of Reference to a single logical qubit}}
  "quanta.x"(%c, %q) <{operandSegmentSizes = array<i32: 0, 1, 1>}> : (i1, !quanta.qubit) -> ()
  return
}

// -----

func.func @target_type(%q: i64) {
  // expected-error @+1 {{operand #0 must be variadic of Reference to a single logical qubit}}
  "quanta.h"(%q) <{operandSegmentSizes = array<i32: 0, 0, 1>}> : (i64) -> ()
  return
}

// -----

func.func @unexpected_result(%q: !quanta.qubit) {
  // expected-error @+1 {{requires zero results}}
  %r = "quanta.h"(%q) <{operandSegmentSizes = array<i32: 0, 0, 1>}> : (!quanta.qubit) -> !quanta.qubit
  return
}

// -----

func.func @invalid_adjoint(%q: !quanta.qubit) {
  // expected-error @+1 {{is_adj}}
  "quanta.s"(%q) <{is_adj = false, operandSegmentSizes = array<i32: 0, 0, 1>}> : (!quanta.qubit) -> ()
  return
}

// -----

func.func @invalid_segments(%q: !quanta.qubit) {
  // expected-error @+1 {{operand}}
  "quanta.h"(%q) <{operandSegmentSizes = array<i32: 0, 0, 2>}> : (!quanta.qubit) -> ()
  return
}
