// RUN: %flagtree_q_opt %s --verify-diagnostics

module {
  // expected-error @+1 {{unregistered operation 'quanta.h'}}
  "quanta.h"() : () -> ()
}
