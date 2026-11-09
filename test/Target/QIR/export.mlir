// RUN: %flagtree_q_translate --mlir-to-qir %s | %FileCheck %s
// RUN: %flagtree_q_opt --convert-quanta-to-qir %s | %flagtree_q_translate --mlir-to-qir | %FileCheck %s

func.func @gates() -> i1 {
  %a = quanta.alloc : !quanta.qubit
  %b = quanta.alloc : !quanta.qubit
  %c = quanta.alloc : !quanta.qubit
  %one = arith.constant 1 : i64
  %two = arith.constant 2 : i64
  %sum = arith.addi %one, %two : i64
  %angle = arith.sitofp %sum : i64 to f64
  %negative = arith.negf %angle : f64
  quanta.h<adj> %a : (!quanta.qubit) -> ()
  quanta.x<adj> %a : (!quanta.qubit) -> ()
  quanta.y<adj> %a : (!quanta.qubit) -> ()
  quanta.z<adj> %a : (!quanta.qubit) -> ()
  quanta.s %a : (!quanta.qubit) -> ()
  quanta.s<adj> %a : (!quanta.qubit) -> ()
  quanta.t %a : (!quanta.qubit) -> ()
  quanta.t<adj> %a : (!quanta.qubit) -> ()
  quanta.rx<adj> (%negative) %a : (f64, !quanta.qubit) -> ()
  quanta.ry (%angle) %a : (f64, !quanta.qubit) -> ()
  quanta.rz<adj> (%angle) %a : (f64, !quanta.qubit) -> ()
  quanta.swap<adj> %a, %b : (!quanta.qubit, !quanta.qubit) -> ()
  quanta.x<adj> [%a] %b : (!quanta.qubit, !quanta.qubit) -> ()
  quanta.x<adj> [%a, %b] %c : (!quanta.qubit, !quanta.qubit, !quanta.qubit) -> ()
  quanta.z<adj> [%a] %c : (!quanta.qubit, !quanta.qubit) -> ()
  %unused = quanta.measure %a : !quanta.qubit -> i1
  %result = quanta.measure %c : !quanta.qubit -> i1
  quanta.dealloc %a : !quanta.qubit
  quanta.dealloc %b : !quanta.qubit
  quanta.dealloc %c : !quanta.qubit
  return %result : i1
}

// CHECK: @gates.output_label = internal constant [7 x i8] c"result\00"
// CHECK: define i64 @gates()
// CHECK: call void @__quantum__rt__initialize(ptr null)
// CHECK-NEXT: br label
// CHECK: call void @__quantum__qis__h__body(ptr null)
// CHECK-NEXT: call void @__quantum__qis__x__body(ptr null)
// CHECK-NEXT: call void @__quantum__qis__y__body(ptr null)
// CHECK-NEXT: call void @__quantum__qis__z__body(ptr null)
// CHECK-NEXT: call void @__quantum__qis__s__body(ptr null)
// CHECK-NEXT: call void @__quantum__qis__s__adj(ptr null)
// CHECK-NEXT: call void @__quantum__qis__t__body(ptr null)
// CHECK-NEXT: call void @__quantum__qis__t__adj(ptr null)
// CHECK-NEXT: call void @__quantum__qis__rx__body(double 3.000000e+00, ptr null)
// CHECK-NEXT: call void @__quantum__qis__ry__body(double 3.000000e+00, ptr null)
// CHECK-NEXT: call void @__quantum__qis__rz__body(double -3.000000e+00, ptr null)
// CHECK-NEXT: call void @__quantum__qis__swap__body(ptr null, ptr inttoptr (i64 1 to ptr))
// CHECK-NEXT: call void @__quantum__qis__cnot__body(ptr null, ptr inttoptr (i64 1 to ptr))
// CHECK-NEXT: call void @__quantum__qis__ccx__body(ptr null, ptr inttoptr (i64 1 to ptr), ptr inttoptr (i64 2 to ptr))
// CHECK-NEXT: call void @__quantum__qis__cz__body(ptr null, ptr inttoptr (i64 2 to ptr))
// CHECK-NEXT: br label
// CHECK: call void @__quantum__qis__mz__body(ptr null, ptr null)
// CHECK-NEXT: call void @__quantum__qis__mz__body(ptr inttoptr (i64 2 to ptr), ptr inttoptr (i64 1 to ptr))
// CHECK-NEXT: br label
// CHECK: call void @__quantum__rt__result_record_output(ptr inttoptr (i64 1 to ptr), ptr @gates.output_label)
// CHECK-NEXT: ret i64 0
// CHECK: declare void @__quantum__qis__mz__body(ptr, ptr writeonly) #[[MEAS:[0-9]+]]
// CHECK: attributes #0 = { "entry_point" "output_labeling_schema"="flagtree_q_v1" "qir_profiles"="base_profile" "required_num_qubits"="3" "required_num_results"="2" }
// CHECK: attributes #[[MEAS]] = { "irreversible" }
// CHECK: !llvm.module.flags =
// CHECK-DAG: !{i32 1, !"qir_major_version", i32 2}
// CHECK-DAG: !{i32 7, !"qir_minor_version", i32 0}
// CHECK-DAG: !{i32 1, !"dynamic_qubit_management", i1 false}
// CHECK-DAG: !{i32 1, !"dynamic_result_management", i1 false}
