import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from flagtree_q import __version__


class CLITest(unittest.TestCase):
    def run_cli(self, *args, input=None, env=None):
        return subprocess.run(
            [sys.executable, "-m", "flagtree_q", *args],
            input=input,
            text=True,
            capture_output=True,
            env=env,
            check=False,
        )

    def test_version_without_native_tool(self):
        env = dict(os.environ, FLAGTREE_Q_OPT="/missing/flagtree-q-opt")
        result = self.run_cli("--version", env=env)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(__version__, result.stdout)

    def test_missing_native_tool(self):
        env = dict(os.environ, FLAGTREE_Q_OPT="/missing/flagtree-q-opt")
        result = self.run_cli("opt", env=env)
        self.assertEqual(result.returncode, 127)
        self.assertIn("FLAGTREE_Q_OPT", result.stderr)

    def test_native_dialect_registration(self):
        result = self.run_cli("opt", "--show-dialects")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("quanta", result.stdout.replace(",", " ").split())

    def test_stdin_roundtrip(self):
        result = self.run_cli("opt", input="module { func.func @empty() { return } }")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("func.func @empty", result.stdout)

    def test_native_diagnostics_and_exit_code(self):
        result = self.run_cli("opt", input='module { "quanta.h"() : () -> () }')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("quanta.h", result.stderr)

    def test_file_paths_with_spaces(self):
        with tempfile.TemporaryDirectory(prefix="flagtree q ") as directory:
            source = Path(directory) / "input module.mlir"
            output = Path(directory) / "output module.mlir"
            source.write_text("module {}", encoding="utf-8")
            result = self.run_cli("opt", str(source), "-o", str(output))
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("module", output.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
