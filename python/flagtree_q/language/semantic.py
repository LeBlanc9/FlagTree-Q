"""Type and ownership rules for the first, straight-line kernel subset."""

from .core import Value, qubit


class Semantic:
    def __init__(self, context, builder):
        self.context = context
        self.builder = builder
        self.owned_qubits = []

    def ir_type(self, dtype):
        return {float: self.context.f64, int: self.context.i64,
                bool: self.context.i1, qubit: self.context.qubit}[dtype]

    def constant(self, value):
        dtype = type(value)
        if dtype is bool:
            handle = self.builder.create_i1(value)
        elif dtype is int:
            if not -(1 << 63) <= value < (1 << 63):
                raise ValueError("integer literal is outside the signed i64 range")
            handle = self.builder.create_i64(value)
        elif dtype is float:
            handle = self.builder.create_f64(value)
        else:
            raise TypeError("only bool, int and float constants are supported")
        return Value(handle, dtype)

    def require_value(self, value):
        if not isinstance(value, Value):
            raise TypeError("expected a scalar or qubit value")
        return value

    def require_qubit(self, value):
        value = self.require_value(value)
        if value.dtype is not qubit:
            raise TypeError("expected a qubit")
        return value

    def convert(self, value, dtype):
        value = self.require_value(value)
        if value.dtype is dtype:
            return value
        if value.dtype is int and dtype is float:
            return Value(self.builder.create_sitofp(value.handle), float)
        raise TypeError(f"cannot convert {value.dtype.__name__} to {dtype.__name__}")

    def binary(self, op, lhs, rhs):
        lhs, rhs = self.require_value(lhs), self.require_value(rhs)
        if lhs.dtype not in (int, float) or rhs.dtype not in (int, float):
            raise TypeError("arithmetic requires int or float operands")
        # Division always yields float; otherwise float is contagious over int.
        float_result = op == "div" or lhs.dtype is float or rhs.dtype is float
        dtype = float if float_result else int
        lhs, rhs = self.convert(lhs, dtype), self.convert(rhs, dtype)
        suffix = "f" if dtype is float else "i"
        handle = getattr(self.builder, "create_" + op + suffix)(lhs.handle, rhs.handle)
        return Value(handle, dtype)

    def unary(self, op, value):
        value = self.require_value(value)
        if value.dtype not in (int, float):
            raise TypeError("unary + and - require int or float operands")
        if op == "plus":
            return value
        if value.dtype is float:
            return Value(self.builder.create_negf(value.handle), float)
        # No integer negate op; lower -x to 0 - x.
        return self.binary("sub", self.constant(0), value)

    def allocate(self):
        value = Value(self.builder.create_alloc(), qubit)
        self.owned_qubits.append(value)
        return value

    def gate(self, gate, args):
        parameters, targets = gate.signature
        expected = parameters + targets + int(gate.controlled)
        if len(args) != expected:
            raise TypeError(f"{gate.name} expects {expected} arguments, got {len(args)}")
        angles = [self.convert(value, float) for value in args[:parameters]]
        # Argument order is angles, then one control group, then targets. The
        # control argument may be a single qubit or a tuple/list of qubits.
        controls = []
        if gate.controlled:
            group = args[parameters]
            controls = list(group) if isinstance(group, tuple) else [group]
            if not controls:
                raise ValueError(".ctrl requires at least one control qubit")
            controls = [self.require_qubit(value) for value in controls]
        target_values = [self.require_qubit(value) for value in args[-targets:]]
        qubits = controls + target_values
        if len({id(value) for value in qubits}) != len(qubits):
            raise ValueError("controls and targets must refer to distinct qubits")
        getattr(self.builder, "create_" + gate.name)(
            *[value.handle for value in angles + target_values],
            controls=[value.handle for value in controls], adj=gate.inverted)

    def measure(self, value):
        return Value(self.builder.create_measure(self.require_qubit(value).handle), bool)

    def reset(self, value):
        self.builder.create_reset(self.require_qubit(value).handle)

    def finish(self, value, result_type):
        if isinstance(value, Value) and value.dtype is qubit:
            raise TypeError("kernels cannot return qubits")
        if result_type is None:
            if value is not None:
                raise TypeError("returning a value requires an int, float "
                                "or bool return annotation")
            results = []
        else:
            if value is None:
                raise TypeError("kernel must return its annotated classical value")
            results = [self.convert(value, result_type).handle]
        # Release locally allocated qubits in reverse allocation order. Borrowed
        # parameters are not owned here and must not be released.
        for qubit_value in reversed(self.owned_qubits):
            self.builder.create_dealloc(qubit_value.handle)
        self.builder.create_return(results)
