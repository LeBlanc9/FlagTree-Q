#include <pybind11/pybind11.h>

void initIR(pybind11::module_ &module);

PYBIND11_MODULE(libflagtree_q, module) {
  module.doc() = "FlagTree-Q native compiler bindings";
  auto ir = module.def_submodule("ir");
  initIR(ir);
}
