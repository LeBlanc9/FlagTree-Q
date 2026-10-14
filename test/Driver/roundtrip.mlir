// RUN: %flagtree_q_opt %s | %flagtree_q_opt | %FileCheck %s

module {
  func.func @classical_parameter(%theta: f64, %condition: i1) -> f64 {
    %one = arith.constant 1.0 : f64
    %result = scf.if %condition -> (f64) {
      %cosine = math.cos %theta : f64
      scf.yield %cosine : f64
    } else {
      %shifted = arith.addf %theta, %one : f64
      scf.yield %shifted : f64
    }
    return %result : f64
  }
}

// CHECK-LABEL: func.func @classical_parameter
// CHECK: arith.constant
// CHECK: scf.if
// CHECK: math.cos
// CHECK: scf.yield
// CHECK: arith.addf
// CHECK: scf.yield
// CHECK: return
