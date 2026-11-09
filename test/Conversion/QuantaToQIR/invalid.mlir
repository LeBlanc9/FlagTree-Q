// RUN: %flagtree_q_opt --convert-quanta-to-qir --split-input-file --verify-diagnostics %s

// expected-error @+1 {{QIR Base Profile entry points cannot have parameters}}
func.func @parameter(%angle: f64) {
  return
}

// -----

// expected-error @+1 {{QIR export requires a single-block function definition}}
func.func private @declaration()

// -----

// expected-error @+1 {{entry point name uses the reserved QIR prefix}}
func.func @__quantum__rt__initialize() {
  return
}

// -----

// expected-error @+1 {{QIR export supports only void or a single measurement result}}
func.func @integer() -> i64 {
  %c = arith.constant 0 : i64
  return %c : i64
}

// -----

func.func @constant_bit() -> i1 {
  %c = arith.constant true
  // expected-error @+1 {{QIR export requires the returned bit to be a measurement result}}
  return %c : i1
}

// -----

func.func @gate_after_measure() {
  %a = quanta.alloc : !quanta.qubit
  %b = quanta.alloc : !quanta.qubit
  %r = quanta.measure %a : !quanta.qubit -> i1
  // expected-error @+1 {{QIR Base Profile requires all gates before measurements}}
  quanta.h %b : (!quanta.qubit) -> ()
  quanta.dealloc %a : !quanta.qubit
  quanta.dealloc %b : !quanta.qubit
  return
}

// -----

func.func @repeated_measure() {
  %q = quanta.alloc : !quanta.qubit
  %r0 = quanta.measure %q : !quanta.qubit -> i1
  // expected-error @+1 {{QIR Base Profile cannot measure a qubit more than once}}
  %r1 = quanta.measure %q : !quanta.qubit -> i1
  quanta.dealloc %q : !quanta.qubit
  return
}

// -----

func.func @reset() {
  %q = quanta.alloc : !quanta.qubit
  // expected-error @+1 {{QIR Base Profile export does not support reset}}
  quanta.reset %q : !quanta.qubit
  quanta.dealloc %q : !quanta.qubit
  return
}

// -----

func.func @controlled_h() {
  %a = quanta.alloc : !quanta.qubit
  %b = quanta.alloc : !quanta.qubit
  // expected-error @+1 {{unsupported controlled gate in the QIR instruction set}}
  quanta.h [%a] %b : (!quanta.qubit, !quanta.qubit) -> ()
  quanta.dealloc %a : !quanta.qubit
  quanta.dealloc %b : !quanta.qubit
  return
}

// -----

// expected-error @+1 {{QIR export requires every allocated qubit to be released}}
func.func @leak() {
  %q = quanta.alloc : !quanta.qubit
  quanta.h %q : (!quanta.qubit) -> ()
  return
}

// -----

func.func @infinite_angle() {
  %q = quanta.alloc : !quanta.qubit
  %inf = arith.constant 0x7FF0000000000000 : f64
  // expected-error @+1 {{QIR export requires finite rotation angles}}
  quanta.rx (%inf) %q : (f64, !quanta.qubit) -> ()
  quanta.dealloc %q : !quanta.qubit
  return
}

// -----

func.func @result_computation() -> i1 {
  %q = quanta.alloc : !quanta.qubit
  %r = quanta.measure %q : !quanta.qubit -> i1
  %true = arith.constant true
  // expected-error @+1 {{unsupported operation in QIR Base Profile export}}
  %inverse = arith.xori %r, %true : i1
  quanta.dealloc %q : !quanta.qubit
  return %inverse : i1
}

// -----

// expected-error @+1 {{QIR export requires a module containing exactly one kernel function}}
module {
  func.func @first() { return }
  func.func @second() { return }
}

// -----

func.func @conditional() -> i1 {
  %q = quanta.alloc : !quanta.qubit
  %r = quanta.measure %q : !quanta.qubit -> i1
  // expected-error @+1 {{unsupported operation in QIR Base Profile export}}
  scf.if %r {
    quanta.x %q : (!quanta.qubit) -> ()
  }
  quanta.dealloc %q : !quanta.qubit
  return %r : i1
}
