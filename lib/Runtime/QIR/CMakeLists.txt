add_mlir_library(FlagTreeQQIRRuntime
  Executor.cpp
  LINK_COMPONENTS AsmParser Core OrcJIT native Support
  LINK_LIBS PUBLIC FlagTreeQQIR
)
