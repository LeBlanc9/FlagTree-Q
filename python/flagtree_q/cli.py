"""A small CLI that forwards MLIR input to the native optimizer driver."""

import argparse
import os
import shutil
import subprocess
import sys
from typing import Optional, Sequence

from . import __version__


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    if args and args[0] == "opt":
        requested = os.environ.get("FLAGTREE_Q_OPT", "flagtree-q-opt")
        executable = shutil.which(requested)
        if executable is None:
            print(
                f"flagtree-q: optimizer not found: {requested!r}. "
                "Build flagtree-q-opt and set FLAGTREE_Q_OPT to its path, "
                "or add its bin directory to PATH.",
                file=sys.stderr,
            )
            return 127
        try:
            # Inherit stdin/stdout/stderr so files, pipes, and diagnostics work
            # exactly as they do when invoking the native tool directly.
            result = subprocess.run([executable, *args[1:]], check=False)
        except OSError as error:
            print(f"flagtree-q: cannot launch optimizer: {error}", file=sys.stderr)
            return 126
        return result.returncode if result.returncode >= 0 else 128 - result.returncode

    parser = argparse.ArgumentParser(
        prog="flagtree-q",
        description="FlagTree-Q compiler tools (project skeleton).",
        epilog="Use 'flagtree-q opt --help' for native MLIR driver options.",
    )
    parser.add_argument("--version", action="version", version=f"FlagTree-Q {__version__}")
    subparsers = parser.add_subparsers(dest="command")
    subparsers.add_parser("opt", help="run the native flagtree-q-opt tool")
    parser.parse_args(args)
    parser.print_help()
    return 0
