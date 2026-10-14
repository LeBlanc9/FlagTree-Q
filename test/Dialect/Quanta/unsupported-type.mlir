// RUN: %flagtree_q_opt %s --verify-diagnostics

// expected-error @+1 {{dialect 'quanta' provides no type parsing hook}}
func.func private @future_qubit(!quanta.qubit)
