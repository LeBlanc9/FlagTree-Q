// RUN: %flagtree_q_translate --mlir-to-qir --split-input-file --verify-diagnostics %s

// A target exporter must not silently treat arbitrary LLVM IR as QIR.
// expected-error @+1 {{QIR export requires one llvm.module_flags operation}}
module {
  llvm.func @unrelated() {
    llvm.return
  }
}

// -----

module {
  // expected-error @+1 {{QIR export requires an already lowered LLVM dialect module}}
  %q = quanta.alloc : !quanta.qubit
}

// -----

// expected-error @+1 {{QIR export requires QIR 2.0 static-resource module flags}}
module {
  llvm.module_flags [#llvm.mlir.module_flag<error, "qir_major_version", 1 : i32>,
                     #llvm.mlir.module_flag<max, "qir_minor_version", 0 : i32>,
                     #llvm.mlir.module_flag<error, "dynamic_qubit_management", false>,
                     #llvm.mlir.module_flag<error, "dynamic_result_management", false>]
}

// -----

// Reject dynamic allocation rather than overwriting its flag during export.
// expected-error @+1 {{QIR export requires QIR 2.0 static-resource module flags}}
module {
  llvm.module_flags [#llvm.mlir.module_flag<error, "qir_major_version", 2 : i32>,
                     #llvm.mlir.module_flag<max, "qir_minor_version", 0 : i32>,
                     #llvm.mlir.module_flag<error, "dynamic_qubit_management", true>,
                     #llvm.mlir.module_flag<error, "dynamic_result_management", false>]
}
