import importlib.util
import os
import subprocess
import sys
import unittest
from unittest.mock import patch

import flagtree_q as fq
import flagtree_q.language as ql
from flagtree_q.backends.qir_runner import _parse_counts, _runner_ir, sample_qir


@fq.kernel
def zero() -> bool:
    q = ql.qubit()
    return ql.measure(q)


@fq.kernel
def one() -> bool:
    q = ql.qubit()
    ql.x(q)
    return ql.measure(q)


@fq.kernel
def random_bit() -> bool:
    q = ql.qubit()
    ql.h(q)
    return ql.measure(q)


@fq.kernel
def bell() -> bool:
    a = ql.qubit()
    b = ql.qubit()
    ql.h(a)
    ql.x.ctrl(a, b)
    return ql.measure(b)


@fq.kernel
def uncompute_bell() -> bool:
    a = ql.qubit()
    b = ql.qubit()
    ql.h(a)
    ql.x.ctrl(a, b)
    ql.x.ctrl(a, b)
    ql.h(a)
    return ql.measure(a)


@fq.kernel
def controlled_x() -> bool:
    a = ql.qubit()
    b = ql.qubit()
    ql.x(a)
    ql.x.ctrl(a, b)
    return ql.measure(b)


@fq.kernel
def controlled_z() -> bool:
    a = ql.qubit()
    b = ql.qubit()
    ql.x(a)
    ql.h(b)
    ql.z.ctrl(a, b)
    ql.h(b)
    return ql.measure(b)


@fq.kernel
def toffoli() -> bool:
    a = ql.qubit()
    b = ql.qubit()
    c = ql.qubit()
    ql.x(a)
    ql.x(b)
    ql.x.ctrl([a, b], c)
    return ql.measure(c)


@fq.kernel
def swap() -> bool:
    a = ql.qubit()
    b = ql.qubit()
    ql.x(a)
    ql.swap(a, b)
    return ql.measure(b)


@fq.kernel
def inverse_pairs() -> bool:
    q = ql.qubit()
    ql.h(q)
    ql.s(q)
    ql.s.inv(q)
    ql.t(q)
    ql.t.inv(q)
    ql.y(q)
    ql.y.inv(q)
    ql.z(q)
    ql.z.inv(q)
    ql.rx(0.7, q)
    ql.rx.inv(0.7, q)
    ql.ry(1.2, q)
    ql.ry.inv(1.2, q)
    ql.rz(-0.3, q)
    ql.rz.inv(-0.3, q)
    ql.h.inv(q)
    return ql.measure(q)


@fq.kernel
def returned_measurement() -> bool:
    a = ql.qubit()
    b = ql.qubit()
    ql.x(a)
    ql.measure(a)
    return ql.measure(b)


@fq.kernel
def discarded_measurement():
    q = ql.qubit()
    ql.h(q)
    ql.measure(q)


@fq.kernel
def empty():
    pass


@fq.kernel
def parameterized(theta: float) -> bool:
    q = ql.qubit()
    ql.rx(theta, q)
    return ql.measure(q)


class SampleAPITest(unittest.TestCase):
    def test_import_does_not_load_native_or_optional_dependencies(self):
        result = subprocess.run(
            [sys.executable, "-c",
             "import sys; import flagtree_q as fq; "
             "assert callable(fq.sample); "
             "assert 'qirrunner' not in sys.modules; "
             "assert 'flagtree_q._C.libflagtree_q' not in sys.modules"],
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_invalid_arguments_fail_before_compilation(self):
        with self.assertRaisesRegex(TypeError, "@kernel"):
            fq.sample(lambda: None)
        cases = [
            ({"shots": True}, TypeError, "shots"),
            ({"shots": 1.5}, TypeError, "shots"),
            ({"shots": 0}, ValueError, "shots"),
            ({"shots": -1}, ValueError, "shots"),
            ({"shots": 2**32}, ValueError, "shots"),
            ({"seed": True}, TypeError, "seed"),
            ({"seed": 1.5}, TypeError, "seed"),
            ({"seed": -1}, ValueError, "seed"),
            ({"seed": 2**64}, ValueError, "seed"),
            ({"backend": "missing"}, ValueError, "backend"),
        ]
        with patch.object(zero, "compile", side_effect=AssertionError("compiled")):
            for options, error, message in cases:
                with self.subTest(options=options):
                    with self.assertRaisesRegex(error, message):
                        fq.sample(zero, **options)

    def test_missing_optional_dependency_has_install_instructions(self):
        with patch.dict(sys.modules, {"qirrunner": None}):
            with self.assertRaisesRegex(ImportError, r"flagtree-q\[qir-runner\]"):
                sample_qir("", shots=1, seed=None)

    def test_failed_or_incomplete_output_never_returns_partial_counts(self):
        result = "OUTPUT\tRESULT\t1\tresult\n"
        complete = "START\n" + result + "END\t0\n"
        cases = [
            "", complete + "START\n", "START\n" + result,
            "START\n" + result + "END\t1\n", "START\nEND\t0\n",
            "START\n" + result + result + "END\t0\n",
            complete.replace("RESULT\t1", "RESULT\t2"),
            complete.replace("RESULT\t1", "INT\t1"),
            complete + complete,
        ]
        for output in cases:
            with self.subTest(output=output):
                with self.assertRaises(RuntimeError):
                    _parse_counts(output, shots=1)

    def test_float_compatibility_preserves_bits_names_and_strings(self):
        prefix = (
            '; double f0x3FE6666666666666\n'
            '@"double f0x3FE6666666666666" = constant [24 x i8] '
            'c"double f0x3FE6666666666666"\n'
            '@"escaped\\22 double f0x3FE6666666666666" = external global double\n'
        )
        constants = ["3FE6666666666666", "BFE6666666666666", "0000000000000001"]
        source = prefix + "\n".join(f"double f0x{bits}" for bits in constants)
        expected = prefix + "\n".join(f"double 0x{bits}" for bits in constants)
        self.assertEqual(_runner_ir(source), expected)
        self.assertEqual(_runner_ir(expected), expected)


@unittest.skipUnless(os.environ.get("FLAGTREE_Q_TEST_BINDINGS") == "1",
                     "configure with FLAGTREE_Q_BUILD_PYTHON_MODULE=ON")
@unittest.skipUnless(importlib.util.find_spec("qirrunner") is not None,
                     "install the qir-runner extra for execution tests")
class QIRRunnerTest(unittest.TestCase):
    def test_constant_results_and_fresh_state_for_each_shot(self):
        self.assertEqual(fq.sample(zero, shots=32), {"0": 32})
        # If shots shared qubit state, successive X gates would alternate bits.
        self.assertEqual(fq.sample(one, shots=32), {"1": 32})

    def test_random_distribution_and_seed_reproducibility(self):
        counts = fq.sample(random_bit, shots=1000, seed=42)
        self.assertEqual(sum(counts.values()), 1000)
        self.assertEqual(set(counts), {"0", "1"})
        self.assertTrue(350 < counts["0"] < 650, counts)
        fq.sample(one, shots=3)  # An intervening call must not affect a seed.
        self.assertEqual(counts, fq.sample(random_bit, shots=1000, seed=42))

    def test_entangled_circuit_and_its_inverse(self):
        counts = fq.sample(bell, shots=1000, seed=42)
        self.assertEqual(sum(counts.values()), 1000)
        self.assertTrue(350 < counts.get("0", 0) < 650, counts)
        self.assertEqual(fq.sample(uncompute_bell, shots=32), {"0": 32})

    def test_controlled_gates_and_swap(self):
        for kernel in (controlled_x, controlled_z, toffoli, swap):
            with self.subTest(kernel=kernel.__name__):
                self.assertEqual(fq.sample(kernel, shots=32), {"1": 32})

    def test_phase_and_rotation_inverse_pairs(self):
        self.assertEqual(fq.sample(inverse_pairs, shots=32), {"0": 32})

    def test_only_the_returned_measurement_contributes_to_counts(self):
        self.assertEqual(fq.sample(returned_measurement, shots=32), {"0": 32})

    def test_void_counts_empty_bitstrings_without_implicit_measurement(self):
        for kernel in (empty, discarded_measurement):
            with self.subTest(kernel=kernel.__name__):
                self.assertEqual(fq.sample(kernel, shots=5), {"": 5})

    def test_qir_restrictions_retain_source_diagnostics(self):
        with self.assertRaisesRegex(ValueError, "entry points cannot have parameters") as caught:
            fq.sample(parameterized)
        self.assertIn("test_sample.py", str(caught.exception))

    def test_runner_failure_propagates(self):
        with self.assertRaises(RuntimeError):
            sample_qir("invalid LLVM IR", shots=1, seed=None)

    def test_seed_boundaries(self):
        for seed in (0, 2**64 - 1):
            with self.subTest(seed=seed):
                self.assertEqual(fq.sample(one, shots=1, seed=seed), {"1": 1})


if __name__ == "__main__":
    unittest.main()
