// RUN: %flagtree_q_opt --show-dialects | %FileCheck %s
// CHECK-DAG: arith
// CHECK-DAG: builtin
// CHECK-DAG: func
// CHECK-DAG: math
// CHECK-DAG: quanta
// CHECK-DAG: scf
