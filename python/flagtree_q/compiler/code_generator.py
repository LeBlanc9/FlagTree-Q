"""Lower the supported Python AST subset without executing the function body."""

import ast
import builtins

from .. import language
from ..language.core import Gate, Value, measure, qubit, reset
from ..language.semantic import Semantic
from .diagnostics import CompilationError


class CodeGenerator(ast.NodeVisitor):
    UNARY_OPERATORS = {ast.UAdd: "plus", ast.USub: "minus"}
    BINARY_OPERATORS = {ast.Add: "add", ast.Sub: "sub",
                        ast.Mult: "mul", ast.Div: "div"}

    def __init__(self, function, source, context, builder):
        self.source = source
        self.builder = builder
        self.semantic = Semantic(context, builder)
        self.globals = dict(function.__globals__)
        for name, cell in zip(function.__code__.co_freevars, function.__closure__ or ()):
            try:
                self.globals[name] = cell.cell_contents
            except ValueError:
                raise source.error(source.function, f"unbound closure variable: {name}") from None
        self.locals = {}
        # Python determines local names for the whole function, even before an
        # assignment. Never fall back to a global DSL symbol shadowed later on.
        self.local_names = {node.id for node in ast.walk(source.function)
                            if isinstance(node, ast.Name) and isinstance(node.ctx, ast.Store)}
        self.result_type = None

    def locate(self, node):
        self.builder.set_location(*self.source.location(node))

    def visit(self, node):
        self.locate(node)
        try:
            return super().visit(node)
        except CompilationError:
            raise
        except (TypeError, ValueError, OverflowError) as error:
            raise self.source.error(node, str(error)) from None

    def generic_visit(self, node):
        raise self.source.error(node, f"unsupported kernel syntax: {type(node).__name__}")

    def lookup(self, name, use_locals=True):
        if use_locals:
            if name in self.locals:
                return self.locals[name]
            if name in self.local_names:
                raise TypeError(f"local variable used before assignment: {name}")
        if name in self.globals:
            return self.globals[name]
        if name in vars(builtins):
            return vars(builtins)[name]
        raise TypeError(f"unknown name: {name}")

    def symbol(self, node, use_locals=True):
        if isinstance(node, ast.Name):
            return self.lookup(node.id, use_locals)
        if isinstance(node, ast.Attribute):
            base = self.symbol(node.value, use_locals)
            if base is language and node.attr in language.__all__:
                return vars(language)[node.attr]
            if type(base) is Gate:
                if node.attr == "inv":
                    return base.inv
                if node.attr == "ctrl":
                    return base.ctrl
                raise TypeError("gate modifiers are .inv and .ctrl")
            if base is measure or base is reset or base is qubit:
                raise TypeError(".inv and .ctrl are only supported on unitary gates")
            raise TypeError("attribute access is limited to language symbols and gate modifiers")
        raise TypeError("expected a named language symbol")

    def annotation(self, node, allow_void=False):
        if node is None:
            if allow_void:
                return None
            raise self.source.error(self.source.function, "kernel parameters need type annotations")
        expression = node
        try:
            if isinstance(expression, ast.Constant) and type(expression.value) is str:
                expression = ast.parse(expression.value, mode="eval").body
            if isinstance(expression, ast.Constant) and expression.value is None:
                dtype = None
            else:
                dtype = self.symbol(expression, use_locals=False)
            if dtype is None and allow_void:
                return None
            if not any(dtype is allowed for allowed in (int, float, bool, qubit)):
                raise TypeError("supported annotations are int, float, bool and ql.qubit")
            if dtype is qubit and allow_void:
                raise TypeError("kernels cannot return qubits")
            return dtype
        except (TypeError, ValueError, SyntaxError) as error:
            raise self.source.error(node, str(error)) from None

    def generate(self):
        node = self.source.function
        args = node.args
        if args.vararg or args.kwarg or args.kwonlyargs or args.defaults:
            raise self.source.error(
                node, "kernel parameters must be positional, without "
                "defaults or variadic arguments")
        parameters = args.posonlyargs + args.args
        self.local_names.update(arg.arg for arg in parameters)
        input_types = [self.annotation(arg.annotation) for arg in parameters]
        self.result_type = self.annotation(node.returns, allow_void=True)
        self.locate(node)
        native_function = self.builder.create_function(
            node.name, [self.semantic.ir_type(dtype) for dtype in input_types],
            [] if self.result_type is None else [self.semantic.ir_type(self.result_type)])
        for argument, dtype, handle in zip(parameters, input_types, native_function.arguments):
            self.locals[argument.arg] = Value(handle, dtype)
        body = node.body
        if (body and isinstance(body[0], ast.Expr)
                and isinstance(body[0].value, ast.Constant)
                and isinstance(body[0].value.value, str)):
            body = body[1:]
        for index, statement in enumerate(body):
            if isinstance(statement, ast.Return) and index != len(body) - 1:
                raise self.source.error(statement, "return must be the final statement of a kernel")
            self.visit(statement)
        if not body or not isinstance(body[-1], ast.Return):
            self.locate(node)
            try:
                self.semantic.finish(None, self.result_type)
            except (TypeError, ValueError) as error:
                raise self.source.error(node, str(error)) from None

    def visit_Name(self, node):
        value = self.lookup(node.id)
        if type(value) in (bool, int, float):
            return self.semantic.constant(value)
        return value

    def visit_Attribute(self, node):
        return self.symbol(node)

    def visit_Constant(self, node):
        if node.value is None:
            return None
        return self.semantic.constant(node.value)

    def visit_List(self, node):
        return tuple(self.visit(value) for value in node.elts)

    visit_Tuple = visit_List

    def visit_Call(self, node):
        if node.keywords or any(isinstance(arg, ast.Starred) for arg in node.args):
            raise TypeError("language calls require positional arguments without unpacking")
        callee = self.symbol(node.func)
        if not (type(callee) is Gate or callee is qubit or callee is measure or callee is reset):
            raise TypeError("only FlagTree-Q language operations can be called inside a kernel")
        args = [self.visit(arg) for arg in node.args]
        self.locate(node)
        if type(callee) is Gate:
            return self.semantic.gate(callee, args)
        if callee is qubit:
            if args:
                raise TypeError("qubit() takes no arguments")
            return self.semantic.allocate()
        if len(args) != 1:
            raise TypeError(f"{callee.name} expects one qubit")
        if callee is measure:
            return self.semantic.measure(args[0])
        return self.semantic.reset(args[0])

    def bind(self, name, value):
        value = self.semantic.require_value(value)
        if name in self.locals and self.locals[name].dtype is not value.dtype:
            raise TypeError(f"assignment changes the type of {name}")
        self.locals[name] = value

    def visit_Assign(self, node):
        if len(node.targets) != 1 or not isinstance(node.targets[0], ast.Name):
            raise TypeError("assignment requires a single local name")
        self.bind(node.targets[0].id, self.visit(node.value))

    def visit_AnnAssign(self, node):
        if not isinstance(node.target, ast.Name) or node.value is None:
            raise TypeError("annotated assignment requires a local name and a value")
        dtype = self.annotation(node.annotation)
        value = self.visit(node.value)
        self.locate(node)
        self.bind(node.target.id, self.semantic.convert(value, dtype))

    def visit_Expr(self, node):
        if not isinstance(node.value, ast.Call):
            raise TypeError("only language calls can be used as expression statements")
        self.visit(node.value)

    def visit_Pass(self, node):
        pass

    def visit_Return(self, node):
        value = self.visit(node.value) if node.value is not None else None
        self.locate(node)
        self.semantic.finish(value, self.result_type)

    def visit_UnaryOp(self, node):
        if type(node.op) not in self.UNARY_OPERATORS:
            raise TypeError("supported unary operators are + and -")
        # Permit the signed i64 minimum: its positive AST literal is out of range.
        if (isinstance(node.operand, ast.Constant)
                and type(node.operand.value) in (int, float)):
            value = node.operand.value
            if isinstance(node.op, ast.USub):
                value = -value
            return self.semantic.constant(value)
        value = self.visit(node.operand)
        self.locate(node)
        return self.semantic.unary(self.UNARY_OPERATORS[type(node.op)], value)

    def visit_BinOp(self, node):
        if type(node.op) not in self.BINARY_OPERATORS:
            raise TypeError("supported binary operators are +, -, * and /")
        lhs, rhs = self.visit(node.left), self.visit(node.right)
        self.locate(node)
        return self.semantic.binary(self.BINARY_OPERATORS[type(node.op)], lhs, rhs)
