"""Build the project-owned C++ extension against an external LLVM/MLIR build."""

import os
from pathlib import Path
import shlex
import subprocess
import sys

from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext


class CMakeBuild(build_ext):
    def build_extension(self, ext):
        import pybind11

        source = Path(__file__).resolve().parent
        output = Path(self.get_ext_fullpath(ext.name)).resolve().parent
        build = Path(self.build_temp).resolve() / ext.name
        build.mkdir(parents=True, exist_ok=True)
        config = "Debug" if self.debug else "Release"
        args = [
            "cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
            f"-DCMAKE_BUILD_TYPE={config}",
            "-DFLAGTREE_Q_BUILD_PYTHON_MODULE=ON",
            "-DFLAGTREE_Q_BUILD_TESTS=OFF",
            f"-DPython3_EXECUTABLE={sys.executable}",
            f"-Dpybind11_DIR={pybind11.get_cmake_dir()}",
            f"-DFLAGTREE_Q_PYTHON_OUTPUT_DIR={output}",
        ]
        if os.environ.get("MLIR_DIR"):
            args.append(f"-DMLIR_DIR={os.environ['MLIR_DIR']}")
        args.extend(shlex.split(os.environ.get("CMAKE_ARGS", "")))
        subprocess.run(args, check=True)
        command = ["cmake", "--build", str(build), "--config", config,
                   "--target", "libflagtree_q"]
        if "CMAKE_BUILD_PARALLEL_LEVEL" not in os.environ:
            command.extend(["--parallel", str(self.parallel or 2)])
        subprocess.run(command, check=True)


setup(
    ext_modules=[Extension("flagtree_q._C.libflagtree_q", sources=[])],
    cmdclass={"build_ext": CMakeBuild},
)
