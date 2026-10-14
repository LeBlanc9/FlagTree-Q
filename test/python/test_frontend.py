from __future__ import annotations

import inspect
import os
from pathlib import Path
import subprocess
import unittest

import flagtree_q as fq
import flagtree_q.language as ql
from flagtree_q.compiler import CompilationError
from flagtree_q.language import h as hadamard


@fq.kernel
def circuit(theta: float) -> bool:
    q0 = ql.qubit()
    q1 = ql.qubit()
    ql.h(q0)
    ql.x.ctrl(q0, q1)
    ql.rx.inv(theta, q1)
    return ql.measure(q1)


@fq.kernel
def modifiers(theta: float):
    q0 = ql.qubit()
    q1 = ql.qubit()
    q2 = ql.qubit()
    q3 = ql.qubit()
    ql.x.ctrl([q0, q1], q2)
    ql.rx.inv.inv(theta, q2)
    ql.ry.ctrl.inv(theta, q0, q2)
    ql.rz.inv.ctrl(theta, q0, q2)
    ql.y(q2)
    ql.z(q2)
    ql.s.inv(q2)
    ql.t.inv(q2)
    ql.swap.ctrl((q0, q1), q2, q3)


@fq.kernel
def arithmetic(theta: float, n: int) -> bool:
    q = ql.qubit()
    angle: float = 1
    angle = (-theta + n) * 2.0 / 3.0 - angle
    ql.rx.inv(angle, q)
    ql.reset(q)
    return ql.measure(q)


@fq.kernel
def integer_arithmetic(n: int) -> int:
    return -n * 2 + 1


@fq.kernel
def integer_division(n: int) -> float:
    return n / 2


@fq.kernel
def integer_minimum() -> int:
    return -9223372036854775808


@fq.kernel
def borrowed(q: ql.qubit) -> bool:
    hadamard(q)
    ql.reset(q)
    return ql.measure(q)


@fq.kernel
def aliases() -> bool:
    q = ql.qubit()
    original = q
    q = ql.qubit()
    ql.x.ctrl(original, q)
    return ql.measure(original)


@fq.kernel
def no_annotation(theta):
    pass


@fq.kernel
def measurement_inverse():
    q = ql.qubit()
    ql.measure.inv(q)


@fq.kernel
def reset_control():
    q = ql.qubit()
    ql.reset.ctrl(q, q)


@fq.kernel
def duplicate_alias():
    q = ql.qubit()
    alias = q
    ql.x.ctrl(alias, q)


@fq.kernel
def empty_controls():
    q = ql.qubit()
    ql.x.ctrl([], q)


@fq.kernel
def boolean_angle():
    q = ql.qubit()
    ql.rx(True, q)


@fq.kernel
def quantum_escape():
    q = ql.qubit()
    return q


@fq.kernel
def invalid_return() -> int:
    return 1.25


@fq.kernel
def missing_return() -> bool:
    q = ql.qubit()


@fq.kernel
def early_return():
    return
    q = ql.qubit()


@fq.kernel
def conditional(flag: bool):
    q = ql.qubit()
    if flag:
        ql.x(q)


@fq.kernel
def loop():
    q = ql.qubit()
    for i in range(3):
        ql.x(q)


@fq.kernel
def wrong_gate_arity():
    q = ql.qubit()
    ql.swap(q)


@fq.kernel
def shadowed_language():
    q = ql.qubit()
    ql = 1


@fq.kernel
def changed_type():
    a = 1
    a = 1.5


@fq.kernel
def oversized_integer() -> int:
    return 9223372036854775808


SIDE_EFFECTS = []


def effect():
    SIDE_EFFECTS.append("called")


class Trap:
    @property
    def h(self):
        SIDE_EFFECTS.append("property")
        return ql.h


trap = Trap()


@fq.kernel
def arbitrary_call():
    effect()


@fq.kernel
def arbitrary_property():
    q = ql.qubit()
    trap.h(q)


@fq.kernel
def annotation_call(value: "effect()"):
    pass


class LanguageTest(unittest.TestCase):
    def test_gate_modifiers_are_immutable(self):
        self.assertFalse(ql.rx.inverted)
        self.assertTrue(ql.rx.inv.inverted)
        self.assertEqual(ql.rx.inv.inv, ql.rx)
        self.assertEqual(ql.rx.ctrl.inv, ql.rx.inv.ctrl)
        self.assertFalse(ql.rx.controlled)
        with self.assertRaisesRegex(ValueError, "list of controls"):
            ql.x.ctrl.ctrl

    def test_language_operations_require_kernel_compilation(self):
        for operation in (ql.qubit, ql.h, ql.rx.inv, ql.measure, ql.reset):
            with self.subTest(operation=operation):
                with self.assertRaisesRegex(RuntimeError, "inside an @kernel"):
                    operation()

    def test_kernel_call_does_not_execute_body(self):
        before = list(SIDE_EFFECTS)
        with self.assertRaisesRegex(RuntimeError, "execution is not available"):
            arbitrary_call()
        self.assertEqual(SIDE_EFFECTS, before)


@unittest.skipUnless(os.environ.get("FLAGTREE_Q_TEST_BINDINGS") == "1",
                     "configure with FLAGTREE_Q_BUILD_PYTHON_MODULE=ON")
class FrontendTest(unittest.TestCase):
    def test_circuit_roundtrips_through_native_optimizer(self):
        module = circuit.compile()
        self.assertTrue(module.verify())
        text = str(module)
        self.assertIn("f64", text)
        self.assertIn("quanta.rx<adj>", text)
        self.assertEqual(text.count("quanta.alloc"), 2)
        self.assertEqual(text.count("quanta.dealloc"), 2)
        self.assertLess(text.index("quanta.measure"), text.index("quanta.dealloc"))
        self.assertLess(text.rindex("quanta.dealloc"), text.index("return"))
        result = subprocess.run(
            [os.environ["FLAGTREE_Q_OPT"], "--canonicalize", "--cse"],
            input=text, text=True, capture_output=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("quanta.rx<adj>", result.stdout)

    def test_modifiers_and_implicit_cleanup(self):
        text = str(modifiers.compile())
        self.assertIn("quanta.rx (", text)
        self.assertNotIn("quanta.rx<adj>", text)
        self.assertIn("quanta.ry<adj>", text)
        self.assertIn("quanta.rz<adj>", text)
        self.assertIn("quanta.swap [", text)
        self.assertEqual(text.count("quanta.dealloc"), 4)

    def test_classical_arithmetic_and_promotions(self):
        text = str(arithmetic.compile())
        for operation in ("arith.negf", "arith.sitofp", "arith.addf", "arith.subf",
                          "arith.mulf", "arith.divf", "quanta.reset"):
            self.assertIn(operation, text)
        text = str(integer_arithmetic.compile())
        for operation in ("arith.subi", "arith.muli", "arith.addi"):
            self.assertIn(operation, text)
        self.assertIn("arith.divf", str(integer_division.compile()))
        self.assertIn("-9223372036854775808", str(integer_minimum.compile()))

    def test_borrowed_qubit_and_imported_gate_alias(self):
        text = str(borrowed.compile())
        self.assertIn("!quanta.qubit", text)
        self.assertIn("quanta.h", text)
        self.assertNotIn("quanta.dealloc", text)

    def test_reassigned_qubits_are_all_released(self):
        text = str(aliases.compile())
        self.assertEqual(text.count("quanta.alloc"), 2)
        self.assertEqual(text.count("quanta.dealloc"), 2)

    def test_closure_constants_are_read_on_each_compile(self):
        angle = 0.5

        @fq.kernel
        def captured():
            q = ql.qubit()
            ql.rx(angle, q)

        first = captured.compile()
        angle = 0.75
        second = captured.compile()
        self.assertIsNot(first, second)
        self.assertNotEqual(str(first), str(second))

    def test_rejects_unsupported_or_invalid_programs(self):
        cases = (
            (no_annotation, "type annotations"),
            (measurement_inverse, "unitary gates"),
            (reset_control, "unitary gates"),
            (duplicate_alias, "distinct qubits"),
            (empty_controls, "at least one"),
            (boolean_angle, "cannot convert bool"),
            (quantum_escape, "cannot return qubits"),
            (invalid_return, "cannot convert float to int"),
            (missing_return, "must return"),
            (early_return, "final statement"),
            (conditional, "If"),
            (loop, "For"),
            (wrong_gate_arity, "expects 2 arguments"),
            (shadowed_language, "used before assignment"),
            (changed_type, "changes the type"),
            (oversized_integer, "i64 range"),
        )
        for kernel, message in cases:
            with self.subTest(kernel=kernel.__name__):
                with self.assertRaisesRegex(CompilationError, message) as caught:
                    kernel.compile()
                self.assertEqual(Path(caught.exception.filename), Path(__file__))

    def test_compilation_does_not_execute_unknown_calls_or_properties(self):
        before = list(SIDE_EFFECTS)
        for kernel in (arbitrary_call, arbitrary_property, annotation_call):
            with self.subTest(kernel=kernel.__name__):
                with self.assertRaises(CompilationError):
                    kernel.compile()
        self.assertEqual(SIDE_EFFECTS, before)

    def test_unicode_columns_and_nested_source_indentation(self):
        @fq.kernel
        def unicode_source(theta: float):
            角度 = theta; ql.rx(角度, 1)

        lines, first = inspect.getsourcelines(unicode_source.function)
        index = next(i for i, line in enumerate(lines) if "ql.rx" in line)
        with self.assertRaisesRegex(CompilationError, "expected a qubit") as caught:
            unicode_source.compile()
        self.assertEqual(caught.exception.lineno, first + index)
        self.assertEqual(caught.exception.offset, lines[index].index("ql.rx") + 1)
        self.assertEqual(caught.exception.text, lines[index])

    def test_bool_and_void_signatures(self):
        @fq.kernel
        def identity(flag: "bool") -> "bool":
            return flag

        @fq.kernel
        def empty() -> None:
            """A documented empty kernel."""
            pass

        self.assertIn("i1", str(identity.compile()))
        self.assertTrue(empty.compile().verify())

    def test_unavailable_source_has_actionable_error(self):
        namespace = {}
        exec(compile("def generated(): pass", "<generated-kernel>", "exec"), namespace)
        with self.assertRaisesRegex(CompilationError, "source is unavailable"):
            fq.kernel(namespace["generated"]).compile()


if __name__ == "__main__":
    unittest.main()
