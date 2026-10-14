#ifndef FLAGTREEQ_DIALECT_QUANTA_IR_QUANTAOPS_TD
#define FLAGTREEQ_DIALECT_QUANTA_IR_QUANTAOPS_TD

include "FlagTreeQ/Dialect/Quanta/IR/QuantaTypes.td"
include "mlir/IR/OpBase.td"
include "mlir/Interfaces/SideEffectInterfaces.td"

class Quanta_Op<string mnemonic, list<Trait> traits = []>
    : Op<Quanta_Dialect, mnemonic, traits>;

// Operations without control or adjoint modifiers.
class Quanta_NonUnitaryOp<string mnemonic, list<Trait> traits>
    : Quanta_Op<mnemonic, traits> {
  let hasVerifier = 1;
}

def Quanta_AllocOp : Quanta_NonUnitaryOp<"alloc",
    [MemoryEffects<[MemAlloc, MemWrite]>]> {
  let summary = "Allocate a fresh qubit initialized to |0>";
  let description = [{
    Creates a fresh logical qubit in |0>, uncorrelated with all existing qubits.
    The result is an owning reference that must be released exactly once on every
    path. Callees may borrow references, but ownership transfer is not supported.
  }];
  let results = (outs Quanta_QubitType:$qubit);
  let assemblyFormat = "attr-dict `:` type($qubit)";
}

def Quanta_MeasureOp : Quanta_NonUnitaryOp<"measure",
    [MemoryEffects<[MemRead, MemWrite]>]> {
  let summary = "Measure one qubit in the Z basis and return a classical bit";
  let description = [{
    Projective measurement in the computational (Z) basis. The joint state
    collapses to P_b rho P_b / probability, where P_b = |b><b| on this qubit.
    The returned i1 is false for |0> and true for |1>.

    The reference remains live; measurement does not reset or release the qubit.
    Read/write effects are conservative, including when the result is unused.
  }];
  let arguments = (ins Quanta_QubitType:$qubit);
  let results = (outs I1:$result);
  let assemblyFormat = " $qubit attr-dict `:` type($qubit) `->` type($result)";
}

def Quanta_ResetOp : Quanta_NonUnitaryOp<"reset",
    [MemoryEffects<[MemRead, MemWrite]>]> {
  let summary = "Reset a live qubit to |0> without returning a measurement";
  let description = [{
    Replaces the qubit's state with |0><0|, preserving the reduced state of all
    other qubits and removing correlations with this one. The reference remains
    valid. There is no classical result; reset is non-unitary and has no adjoint.
  }];
  let arguments = (ins Quanta_QubitType:$qubit);
  let assemblyFormat = "$qubit attr-dict `:` type($qubit)";
}

def Quanta_DeallocOp : Quanta_NonUnitaryOp<"dealloc",
    [MemoryEffects<[MemFree, MemRead, MemWrite]>]> {
  let summary = "Discard an owned qubit and end its lifetime";
  let description = [{
    Releases a qubit allocated by the current function and invalidates all its
    aliases; using or releasing them again is an error. Function parameters are
    borrowed and must not be released by the callee. The input need not be in |0>.

    Verification catches direct releases of function arguments and later uses of
    the same SSA value in the same block. Aliasing, escape, leaks, and cross-block
    correctness require separate analysis and are not proven here.
  }];
  let arguments = (ins Quanta_QubitType:$qubit);
  let assemblyFormat = "$qubit attr-dict `:` type($qubit)";
}

// Unitary gate: parameters, controls, targets, and an optional adjoint marker.
class Quanta_GateOp<string mnemonic, int numParameters, int numTargets>
    : Quanta_Op<mnemonic,
         [AttrSizedOperandSegments, MemoryEffects<[MemRead, MemWrite]>]> {
  let arguments = (ins Variadic<F64>:$parameters,
                       Variadic<Quanta_QubitType>:$controls,
                       Variadic<Quanta_QubitType>:$targets,
                       UnitAttr:$is_adj);
  let results = (outs);
  let assemblyFormat = [{
    (`<` `adj` $is_adj^ `>`)? (`(` $parameters^ `)`)?
    (`[` $controls^ `]`)? $targets
    `:` functional-type(operands, results) attr-dict
  }];
  let hasVerifier = 1;
  let extraClassDefinition = [{
    ::mlir::LogicalResult $cppClass::verify() {
      return verifyGate(getOperation(), getParameters(), getControls(),
                        getTargets(), }] # numParameters # ", " # numTargets # [{);
    }
  }];
  let description = [{
    Updates the quantum state through the target references and returns no
    results. Angles are f64 operands in radians. Absent `is_adj` applies U;
    present applies U^dagger. With controls, U acts on the subspace where every
    control is |1>.

    All control and target references must denote distinct logical qubits; the
    verifier rejects repeated SSA values, but proving non-aliasing is the
    caller's obligation. Read/write effects are conservative to preserve ordering.
  }];
}

def Quanta_HOp : Quanta_GateOp<"h", 0, 1> {
  let summary = "Hadamard gate: (X + Z) / sqrt(2)";
}

def Quanta_XOp : Quanta_GateOp<"x", 0, 1> {
  let summary = "Pauli X gate: exchanges |0> and |1>";
}

def Quanta_YOp : Quanta_GateOp<"y", 0, 1> {
  let summary = "Pauli Y gate: |0> -> i|1>, |1> -> -i|0>";
}

def Quanta_ZOp : Quanta_GateOp<"z", 0, 1> {
  let summary = "Pauli Z gate: diag(1, -1)";
}

def Quanta_SOp : Quanta_GateOp<"s", 0, 1> {
  let summary = "S phase gate: diag(1, i)";
}

def Quanta_TOp : Quanta_GateOp<"t", 0, 1> {
  let summary = "T phase gate: diag(1, exp(i*pi/4))";
}

def Quanta_RXOp : Quanta_GateOp<"rx", 1, 1> {
  let summary = "X rotation: exp(-i*theta*X/2)";
}

def Quanta_RYOp : Quanta_GateOp<"ry", 1, 1> {
  let summary = "Y rotation: exp(-i*theta*Y/2)";
}

def Quanta_RZOp : Quanta_GateOp<"rz", 1, 1> {
  let summary = "Z rotation: exp(-i*theta*Z/2)";
}

def Quanta_SwapOp : Quanta_GateOp<"swap", 0, 2> {
  let summary = "Exchange the states of two target qubits";
}

#endif
