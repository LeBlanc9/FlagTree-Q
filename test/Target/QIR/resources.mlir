// RUN: %flagtree_q_translate --mlir-to-qir --split-input-file %s | %FileCheck %s
// RUN: %flagtree_q_opt --convert-quanta-to-qir --split-input-file %s | %flagtree_q_translate --mlir-to-qir --split-input-file | %FileCheck %s

func.func @discard_entangled() -> i1 {
  %a = quanta.alloc : !quanta.qubit
  %b = quanta.alloc : !quanta.qubit
  quanta.h %a : (!quanta.qubit) -> ()
  quanta.x [%a] %b : (!quanta.qubit, !quanta.qubit) -> ()
  quanta.dealloc %a : !quanta.qubit
  %c = quanta.alloc : !quanta.qubit
  quanta.x %c : (!quanta.qubit) -> ()
  %unused = quanta.measure %b : !quanta.qubit -> i1
  %r = quanta.measure %c : !quanta.qubit -> i1
  quanta.dealloc %b : !quanta.qubit
  quanta.dealloc %c : !quanta.qubit
  return %r : i1
}

// CHECK-LABEL: define i64 @discard_entangled()
// CHECK: call void @__quantum__qis__cnot__body(ptr null, ptr inttoptr (i64 1 to ptr))
// The fresh qubit must not reuse the discarded, potentially entangled qubit 0.
// CHECK-NEXT: call void @__quantum__qis__x__body(ptr inttoptr (i64 2 to ptr))
// CHECK: call void @__quantum__qis__mz__body(ptr inttoptr (i64 1 to ptr), ptr null)
// CHECK-NEXT: call void @__quantum__qis__mz__body(ptr inttoptr (i64 2 to ptr), ptr inttoptr (i64 1 to ptr))
// CHECK: call void @__quantum__rt__result_record_output(ptr inttoptr (i64 1 to ptr), ptr @discard_entangled.output_label)
// CHECK: "required_num_qubits"="3" "required_num_results"="2"

// -----

func.func @discard_result() {
  %q = quanta.alloc : !quanta.qubit
  %r = quanta.measure %q : !quanta.qubit -> i1
  quanta.dealloc %q : !quanta.qubit
  return
}

// CHECK-LABEL: define i64 @discard_result()
// CHECK: call void @__quantum__qis__mz__body(ptr null, ptr null)
// CHECK: call void @__quantum__rt__tuple_record_output(i64 0, ptr @discard_result.output_label)
// CHECK-NEXT: ret i64 0
// CHECK: "required_num_qubits"="1" "required_num_results"="1"
