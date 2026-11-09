"""Capture a Python function without executing its kernel body."""

import functools
import inspect


class Kernel:
    def __init__(self, function):
        if not inspect.isfunction(function) or inspect.iscoroutinefunction(function):
            raise TypeError("@kernel requires an ordinary Python function")
        self.function = function
        functools.update_wrapper(self, function, updated=())

    def compile(self):
        """Compile the function's source to a verified native MLIR module."""
        from ..compiler import compile_kernel
        return compile_kernel(self.function)

    def __call__(self, *args, **kwargs):
        raise RuntimeError(
            "direct kernel execution is not available; use fq.sample(kernel) "
            "to sample or .compile() to generate MLIR"
        )


def kernel(function):
    return Kernel(function)
