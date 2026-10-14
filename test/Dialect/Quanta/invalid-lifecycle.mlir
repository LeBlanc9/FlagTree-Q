// RUN: %flagtree_q_opt %s --split-input-file --verify-diagnostics

func.func @bad_alloc_type() {
  // expected-error @+1 {{result #0 must be Reference to a single logical qubit}}
  %q = "quanta.alloc"() : () -> i64
  return
}

// -----

func.func @alloc_with_input(%q: !quanta.qubit) {
  // expected-error @+1 {{requires zero operands}}
  %r = "quanta.alloc"(%q) : (!quanta.qubit) -> !quanta.qubit
  return
}

// -----

func.func @bad_measure_input(%x: i1) {
  // expected-error @+1 {{operand #0 must be Reference to a single logical qubit}}
  %bit = "quanta.measure"(%x) : (i1) -> i1
  return
}

// -----

func.func @bad_measure_result(%q: !quanta.qubit) {
  // expected-error @+1 {{result #0 must be 1-bit signless integer}}
  %bit = "quanta.measure"(%q) : (!quanta.qubit) -> i32
  return
}

// -----

func.func @measure_without_result(%q: !quanta.qubit) {
  // expected-error @+1 {{requires one result}}
  "quanta.measure"(%q) : (!quanta.qubit) -> ()
  return
}

// -----

func.func @measure_with_quantum_control(%c: !quanta.qubit, %q: !quanta.qubit) {
  // expected-error @+1 {{requires a single operand}}
  %bit = "quanta.measure"(%c, %q) : (!quanta.qubit, !quanta.qubit) -> i1
  return
}

// -----

func.func @bad_reset_input(%x: f64) {
  // expected-error @+1 {{operand #0 must be Reference to a single logical qubit}}
  "quanta.reset"(%x) : (f64) -> ()
  return
}

// -----

func.func @reset_with_result(%q: !quanta.qubit) {
  // expected-error @+1 {{requires zero results}}
  %r = "quanta.reset"(%q) : (!quanta.qubit) -> !quanta.qubit
  return
}

// -----

func.func @bad_dealloc_input(%x: index) {
  // expected-error @+1 {{operand #0 must be Reference to a single logical qubit}}
  "quanta.dealloc"(%x) : (index) -> ()
  return
}

// -----

func.func @dealloc_with_result() {
  %q = quanta.alloc : !quanta.qubit
  // expected-error @+1 {{requires zero results}}
  %r = "quanta.dealloc"(%q) : (!quanta.qubit) -> i1
  return
}

// -----

func.func @alloc_adjoint() {
  // expected-error @+1 {{does not support the 'is_adj' modifier}}
  %q = "quanta.alloc"() {is_adj} : () -> !quanta.qubit
  quanta.dealloc %q : !quanta.qubit
  return
}

// -----

func.func @measure_adjoint(%q: !quanta.qubit) {
  // expected-error @+1 {{does not support the 'is_adj' modifier}}
  %bit = "quanta.measure"(%q) {is_adj} : (!quanta.qubit) -> i1
  return
}

// -----

func.func @reset_adjoint(%q: !quanta.qubit) {
  // expected-error @+1 {{does not support the 'is_adj' modifier}}
  "quanta.reset"(%q) {is_adj} : (!quanta.qubit) -> ()
  return
}

// -----

func.func @dealloc_adjoint() {
  %q = quanta.alloc : !quanta.qubit
  // expected-error @+1 {{does not support the 'is_adj' modifier}}
  "quanta.dealloc"(%q) {is_adj} : (!quanta.qubit) -> ()
  return
}

// -----

func.func @release_borrowed(%q: !quanta.qubit) {
  // expected-error @+1 {{cannot deallocate a borrowed function argument}}
  quanta.dealloc %q : !quanta.qubit
  return
}

// -----

func.func @release_borrowed_in_region(%q: !quanta.qubit, %cond: i1) {
  scf.if %cond {
    // expected-error @+1 {{cannot deallocate a borrowed function argument}}
    quanta.dealloc %q : !quanta.qubit
  }
  return
}

// -----

func.func @double_release() {
  %q = quanta.alloc : !quanta.qubit
  // expected-error @+1 {{qubit is used after this deallocation in the same block}}
  quanta.dealloc %q : !quanta.qubit
  quanta.dealloc %q : !quanta.qubit
  return
}

// -----

func.func @gate_after_release() {
  %q = quanta.alloc : !quanta.qubit
  // expected-error @+1 {{qubit is used after this deallocation in the same block}}
  quanta.dealloc %q : !quanta.qubit
  quanta.h %q : (!quanta.qubit) -> ()
  return
}

// -----

func.func @measure_after_release() -> i1 {
  %q = quanta.alloc : !quanta.qubit
  // expected-error @+1 {{qubit is used after this deallocation in the same block}}
  quanta.dealloc %q : !quanta.qubit
  %bit = quanta.measure %q : !quanta.qubit -> i1
  return %bit : i1
}

// -----

func.func @reset_after_release() {
  %q = quanta.alloc : !quanta.qubit
  // expected-error @+1 {{qubit is used after this deallocation in the same block}}
  quanta.dealloc %q : !quanta.qubit
  quanta.reset %q : !quanta.qubit
  return
}

// -----

func.func @return_released_reference() -> !quanta.qubit {
  %q = quanta.alloc : !quanta.qubit
  // expected-error @+1 {{qubit is used after this deallocation in the same block}}
  quanta.dealloc %q : !quanta.qubit
  return %q : !quanta.qubit
}
