// RUN: %flagtree_q_opt %s --split-input-file --verify-diagnostics

// Qubit references have no size or other type parameters.
// expected-error @+1 {{qubit type does not accept parameters}}
func.func private @parameterized_qubit(!quanta.qubit<2>)

// -----

// expected-error @+1 {{qubit type does not accept parameters}}
func.func private @empty_parameters(!quanta.qubit<>)

// -----

// The generic spelling must not bypass the parameter check.
// expected-error @+1 {{qubit type does not accept parameters}}
func.func private @generic_parameters(!quanta<qubit<2>>)

// -----

// Qubit references remain distinct from classical types during verification.
func.func @incorrect_return(%qubit: !quanta.qubit) -> f64 {
  // expected-error @+1 {{type of return operand 0}}
  return %qubit : !quanta.qubit
}
