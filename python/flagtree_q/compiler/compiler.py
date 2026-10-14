"""Compile a kernel in a fresh native MLIR context."""

from .diagnostics import Source


def compile_kernel(function):
    from .code_generator import CodeGenerator

    source = Source(function)
    try:
        from .._C.libflagtree_q import ir
    except ImportError as error:
        raise ImportError(
            "FlagTree-Q native bindings are unavailable; build with "
            "FLAGTREE_Q_BUILD_PYTHON_MODULE=ON for this Python interpreter"
        ) from error
    context = ir.Context()
    module = ir.Module(context)
    builder = ir.Builder(module)
    generator = CodeGenerator(function, source, context, builder)
    generator.generate()
    try:
        module.verify()
    except ValueError as error:
        raise source.error(source.function, str(error)) from None
    return module
