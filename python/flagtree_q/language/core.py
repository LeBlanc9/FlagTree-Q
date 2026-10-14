"""Language symbols and values used while lowering Python AST to MLIR."""

from dataclasses import dataclass, replace


def _outside_kernel():
    raise RuntimeError("quantum operations can only appear inside an @kernel function")


class qubit:
    """A borrowed qubit parameter type, or a fresh local qubit via qubit()."""

    def __new__(cls):
        _outside_kernel()


_GATE_SIGNATURES = {
    "h": (0, 1), "x": (0, 1), "y": (0, 1), "z": (0, 1),
    "s": (0, 1), "t": (0, 1),
    "rx": (1, 1), "ry": (1, 1), "rz": (1, 1), "swap": (0, 2),
}


@dataclass(frozen=True)
class Gate:
    name: str
    inverted: bool = False
    controlled: bool = False

    @property
    def signature(self):
        return _GATE_SIGNATURES[self.name]

    @property
    def inv(self):
        """Apply the inverse unitary; taking the inverse twice cancels."""
        return replace(self, inverted=not self.inverted)

    @property
    def ctrl(self):
        """Add controls after angle parameters and before target qubits."""
        if self.controlled:
            raise ValueError("use one .ctrl with a list of controls")
        return replace(self, controlled=True)

    def __call__(self, *args, **kwargs):
        _outside_kernel()


@dataclass(frozen=True)
class Builtin:
    name: str

    def __call__(self, *args, **kwargs):
        _outside_kernel()


@dataclass(frozen=True, eq=False)
class Value:
    handle: object
    dtype: type

    def __bool__(self):
        raise TypeError("a runtime kernel value cannot be used as a Python bool")


h = Gate("h")
x = Gate("x")
y = Gate("y")
z = Gate("z")
s = Gate("s")
t = Gate("t")
rx = Gate("rx")
ry = Gate("ry")
rz = Gate("rz")
swap = Gate("swap")
measure = Builtin("measure")
reset = Builtin("reset")
