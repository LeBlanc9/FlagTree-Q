"""Source retrieval and Python/MLIR diagnostic locations."""

import ast
import inspect
import textwrap


class CompilationError(SyntaxError):
    """Unsupported syntax or invalid kernel semantics, with source location."""


class Source:
    def __init__(self, function):
        self.filename = inspect.getsourcefile(function) or function.__code__.co_filename
        try:
            self.lines, self.start_line = inspect.getsourcelines(function)
        except (OSError, TypeError) as error:
            raise CompilationError(
                "kernel source is unavailable; define the kernel in a Python source file"
            ) from error
        self.text = textwrap.dedent("".join(self.lines))
        self.dedented_lines = self.text.splitlines()
        tree = ast.parse(self.text, filename=self.filename)
        functions = [node for node in tree.body if isinstance(node, ast.FunctionDef)]
        if len(functions) != 1 or functions[0].name != function.__name__:
            raise CompilationError("expected the source of one ordinary function")
        self.function = functions[0]

    def location(self, node):
        index = node.lineno - 1
        original = self.lines[index].rstrip("\r\n")
        dedented = self.dedented_lines[index]
        removed = len(original.encode("utf-8")) - len(dedented.encode("utf-8"))
        # AST columns count UTF-8 bytes; SyntaxError offsets count characters.
        prefix = original.encode("utf-8")[:removed + node.col_offset]
        column = len(prefix.decode("utf-8")) + 1
        return self.filename, self.start_line + index, column

    def error(self, node, message):
        filename, line, column = self.location(node)
        return CompilationError(message, (filename, line, column,
                                           self.lines[node.lineno - 1]))
