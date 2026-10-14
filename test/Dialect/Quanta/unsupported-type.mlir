// RUN: %flagtree_q_opt %s --split-input-file --verify-diagnostics

// expected-error @+1 {{type `qreg` in dialect `quanta`}}
func.func private @future_register(!quanta.qreg<2>)

// -----

// expected-error @+1 {{type `qubit_value` in dialect `quanta`}}
func.func private @future_linear_value(!quanta.qubit_value)

// -----

// expected-error @+1 {{type `qubti` in dialect `quanta`}}
func.func private @misspelled_type(!quanta.qubti)
