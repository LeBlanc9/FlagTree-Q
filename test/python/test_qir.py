import os
import subprocess
import unittest

import flagtree_q as fq
import flagtree_q.language as ql


@fq.kernel
def bell() -> bool:
    a = ql.qubit()
    b = ql.qubit()
    ql.h(a)
    ql.x.ctrl(a, b)
    return ql.measure(b)


@fq.kernel
def constant_angle() -> bool:
    q = ql.qubit()
    ql.rx.inv(-(1 + 2) / 2, q)
    return ql.measure(q)


@fq.kernel
def parameterized(theta: float):
    q = ql.qubit()
    ql.rx(theta, q)


@fq.kernel
def empty():
    pass


@unittest.skipUnless(os.environ.get("FLAGTREE_Q_TEST_BINDINGS") == "1",
                     "configure with FLAGTREE_Q_BUILD_PYTHON_MODULE=ON")
class QIRTest(unittest.TestCase):
    def test_dsl_export_matches_cli_and_preserves_source(self):
        module = bell.compile()
        before = str(module)
        qir = module.to_qir()
        result = subprocess.run(
            [os.environ["FLAGTREE_Q_TRANSLATE"], "--mlir-to-qir"],
            input=before, text=True, capture_output=True, check=True)
        self.assertEqual(qir, result.stdout)
        self.assertEqual(before, str(module))
        self.assertTrue(module.verify())
        self.assertEqual(qir, module.to_qir())
        self.assertIn('"required_num_qubits"="2"', qir)
        self.assertIn('"required_num_results"="1"', qir)
        self.assertIn("call void @__quantum__qis__cnot__body", qir)
        self.assertIn("call void @__quantum__rt__result_record_output", qir)
        self.assertIn("ret i64 0", qir)

    def test_bitcode_is_binary_and_matches_cli(self):
        module = bell.compile()
        bitcode = module.to_qir_bitcode()
        self.assertIsInstance(bitcode, bytes)
        self.assertTrue(bitcode.startswith(b"BC\xc0\xde"))
        result = subprocess.run(
            [os.environ["FLAGTREE_Q_TRANSLATE"], "--mlir-to-qir-bitcode"],
            input=str(module).encode(), capture_output=True, check=True)
        self.assertEqual(bitcode, result.stdout)

    def test_constant_folding_and_double_negative_inverse(self):
        qir = constant_angle.compile().to_qir()
        self.assertIn("@__quantum__qis__rx__body(double 1.500000e+00, ptr null)", qir)
        self.assertNotIn(" fdiv ", qir)

    def test_staged_cli_export_matches_python_and_direct_cli(self):
        module = constant_angle.compile()
        lowered = subprocess.run(
            [os.environ["FLAGTREE_Q_OPT"], "--convert-quanta-to-qir"],
            input=str(module).encode(), capture_output=True, check=True).stdout
        for flag, expected in (
            ("--mlir-to-qir", module.to_qir().encode()),
            ("--mlir-to-qir-bitcode", module.to_qir_bitcode()),
        ):
            with self.subTest(flag=flag):
                staged = subprocess.run(
                    [os.environ["FLAGTREE_Q_TRANSLATE"], flag],
                    input=lowered, capture_output=True, check=True)
                direct = subprocess.run(
                    [os.environ["FLAGTREE_Q_TRANSLATE"], flag],
                    input=str(module).encode(), capture_output=True, check=True)
                self.assertEqual(staged.stdout, expected)
                self.assertEqual(direct.stdout, expected)

    def test_void_kernel_records_empty_tuple(self):
        qir = empty.compile().to_qir()
        self.assertIn('"required_num_qubits"="0"', qir)
        self.assertIn('"required_num_results"="0"', qir)
        self.assertIn("@__quantum__rt__tuple_record_output(i64 0, ptr @empty.output_label)", qir)

    def test_failure_preserves_source_and_has_location(self):
        module = parameterized.compile()
        before = str(module)
        with self.assertRaisesRegex(ValueError, "entry points cannot have parameters") as caught:
            module.to_qir()
        self.assertIn("test_qir.py", str(caught.exception))
        self.assertEqual(before, str(module))
        self.assertTrue(module.verify())

    def test_export_keeps_native_handles_alive_and_usable(self):
        from flagtree_q._C.libflagtree_q import ir

        context = ir.Context()
        module = ir.Module(context)
        builder = ir.Builder(module)
        function = builder.create_function("handles", results=[context.i1])
        qubit = builder.create_alloc()
        builder.create_h(qubit)
        result = builder.create_measure(qubit)
        builder.create_dealloc(qubit)
        builder.create_return([result])
        module.to_qir()
        module.to_qir_bitcode()
        self.assertEqual(function.name, "handles")
        self.assertEqual(result.type, context.i1)
        self.assertEqual(qubit.type, context.qubit)
        self.assertTrue(module.verify())


if __name__ == "__main__":
    unittest.main()
