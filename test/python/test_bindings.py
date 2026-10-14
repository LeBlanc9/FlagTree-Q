import gc
import os
import subprocess
import unittest


@unittest.skipUnless(os.environ.get("FLAGTREE_Q_TEST_BINDINGS") == "1",
                     "configure with FLAGTREE_Q_BUILD_PYTHON_MODULE=ON")
class BindingsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from flagtree_q._C.libflagtree_q import ir
        cls.ir = ir

    def setUp(self):
        self.context = self.ir.Context()
        self.module = self.ir.Module(self.context)
        self.builder = self.ir.Builder(self.module)

    def test_parameterized_circuit_and_optimizer_roundtrip(self):
        fn = self.builder.create_function("circuit", [self.context.f64],
                                          [self.context.i1])
        theta, = fn.arguments
        q0 = self.builder.create_alloc()
        q1 = self.builder.create_alloc()
        self.builder.create_h(q0)
        self.builder.create_x(q1, controls=[q0])
        self.builder.create_rx(theta, q1, controls=[q0], adj=True)
        bit = self.builder.create_measure(q1)
        self.builder.create_reset(q1)
        self.builder.create_dealloc(q1)
        self.builder.create_dealloc(q0)
        self.builder.create_return([bit])
        self.assertTrue(self.module.verify())
        self.assertEqual(bit.type, self.context.i1)
        text = str(self.module)
        self.assertIn("quanta.rx<adj>", text)
        result = subprocess.run(
            [os.environ["FLAGTREE_Q_OPT"], "--canonicalize", "--cse"],
            input=text, text=True, capture_output=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("quanta.measure", result.stdout)
        self.assertIn("quanta.rx<adj>", result.stdout)

    def test_all_gates_with_multiple_controls_and_adjoint(self):
        self.builder.create_function("gates")
        q0, q1, q2, q3 = [self.builder.create_alloc() for _ in range(4)]
        theta = self.builder.create_f64(-0.5)
        for name in ("h", "x", "y", "z", "s", "t"):
            getattr(self.builder, "create_" + name)(q2, controls=[q0, q1], adj=True)
        for name in ("rx", "ry", "rz"):
            getattr(self.builder, "create_" + name)(theta, q2,
                                                    controls=[q0, q1], adj=True)
        self.builder.create_swap(q2, q3, controls=[q0, q1], adj=True)
        for qubit in (q3, q2, q1, q0):
            self.builder.create_dealloc(qubit)
        self.builder.create_return()
        self.assertTrue(self.module.verify())
        self.assertEqual(str(self.module).count("<adj>"), 10)

    def test_rejected_operations_leave_builder_usable(self):
        self.builder.create_function("invalid_gate")
        qubit = self.builder.create_alloc()
        with self.assertRaisesRegex(ValueError, "qubit"):
            self.builder.create_x(qubit, controls=[qubit])
        with self.assertRaisesRegex(ValueError, "f64|64-bit float"):
            self.builder.create_rx(qubit, qubit)
        angle = self.builder.create_f64(0.5)
        with self.assertRaisesRegex(ValueError, "qubit"):
            self.builder.create_measure(angle)
        self.builder.create_h(qubit)
        self.builder.create_dealloc(qubit)
        self.builder.create_return()
        self.assertTrue(self.module.verify())
        self.assertNotIn("quanta.rx", str(self.module))

    def test_cross_context_and_cross_module_handles(self):
        other_context = self.ir.Context()
        with self.assertRaisesRegex(ValueError, "different context"):
            self.builder.create_function("bad", [other_context.f64])
        self.builder.create_function("local")
        other_module = self.ir.Module(self.context)
        other_builder = self.ir.Builder(other_module)
        other_fn = other_builder.create_function("other")
        other_value = other_builder.create_alloc()
        with self.assertRaisesRegex(ValueError, "different module"):
            self.builder.create_h(other_value)
        with self.assertRaisesRegex(ValueError, "different module"):
            self.builder.set_insertion_point(other_fn)
        self.builder.create_return()
        self.assertTrue(self.module.verify())

    def test_cross_function_values_and_insertion_point(self):
        first = self.builder.create_function("first")
        qubit = self.builder.create_alloc()
        self.builder.create_function("second")
        with self.assertRaisesRegex(ValueError, "different function"):
            self.builder.create_h(qubit)
        self.builder.create_return()
        self.builder.set_insertion_point(first)
        self.builder.create_dealloc(qubit)
        self.builder.create_return()
        self.assertTrue(self.module.verify())

    def test_handles_retain_owners(self):
        context = self.ir.Context()
        module = self.ir.Module(context)
        builder = self.ir.Builder(module)
        fn = builder.create_function("keep_alive", [context.f64])
        value, = fn.arguments
        del context, module, builder, fn
        gc.collect()
        self.assertEqual(str(value.type), "f64")
        retained_type = value.type
        del value
        gc.collect()
        self.assertEqual(str(retained_type), "f64")

    def test_insertion_requires_unterminated_function(self):
        with self.assertRaisesRegex(ValueError, "create a function"):
            self.builder.create_alloc()
        self.builder.create_function("empty")
        with self.assertRaisesRegex(ValueError, "already exists"):
            self.builder.create_function("empty")
        self.builder.create_return()
        with self.assertRaisesRegex(ValueError, "terminator"):
            self.builder.create_alloc()
        with self.assertRaisesRegex(ValueError, "terminator"):
            self.builder.create_return()
        self.assertTrue(self.module.verify())

    def test_return_type_checks_and_constants(self):
        self.builder.create_function("constants", results=[self.context.i1,
                                                           self.context.i64])
        bit = self.builder.create_i1(True)
        integer = self.builder.create_i64(-7)
        with self.assertRaises(TypeError):
            self.builder.create_i1(2)
        with self.assertRaises(ValueError):
            self.builder.create_return([integer, bit])
        self.builder.create_return([bit, integer])
        self.assertTrue(self.module.verify())

    def test_verifier_reports_lifetime_errors(self):
        fn = self.builder.create_function("lifetime", [self.context.qubit])
        borrowed, = fn.arguments
        with self.assertRaisesRegex(ValueError, "borrowed"):
            self.builder.create_dealloc(borrowed)
        owned = self.builder.create_alloc()
        self.builder.create_dealloc(owned)
        self.builder.create_h(owned)
        self.builder.create_return()
        with self.assertRaisesRegex(ValueError, "dealloc|release"):
            self.module.verify()

    def test_source_locations_and_unfinished_module(self):
        self.builder.set_location("量子.py", 8, 3)
        self.builder.create_function("located")
        qubit = self.builder.create_alloc()
        with self.assertRaises(ValueError) as error:
            self.builder.create_h(qubit, controls=[qubit])
        self.assertIn("量子.py", str(error.exception))
        with self.assertRaises(ValueError):
            self.module.verify()
        self.builder.create_dealloc(qubit)
        self.builder.create_return()
        self.assertTrue(self.module.verify())
        # MLIR's textual printer escapes non-ASCII bytes in filenames.
        self.assertIn(':8:3', self.module.to_string(debug_info=True))

    def test_arithmetic_rejects_wrong_types_without_corrupting_ir(self):
        self.builder.create_function("arithmetic", results=[self.context.f64])
        integer = self.builder.create_i64(3)
        floating = self.builder.create_f64(0.5)
        for operation, args in ((self.builder.create_addf, (integer, floating)),
                                (self.builder.create_addi, (floating, integer)),
                                (self.builder.create_negf, (integer,)),
                                (self.builder.create_sitofp, (floating,))):
            with self.assertRaises(ValueError):
                operation(*args)
        promoted = self.builder.create_sitofp(integer)
        result = self.builder.create_addf(promoted, floating)
        self.builder.create_return([result])
        self.assertTrue(self.module.verify())


if __name__ == "__main__":
    unittest.main()
