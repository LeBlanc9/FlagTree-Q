## 工程结构

```text
include/FlagTreeQ/Dialect/Quanta/IR/   # 公开头文件、TableGen 定义
lib/Dialect/Quanta/IR/                # dialect 实现
lib/Dialect/Quanta/Transforms/        # 量子分析与优化
lib/Conversion/                      # 目标转换
tools/flagtree-q-opt/                 # 原生 MLIR 工具
python/flagtree_q/                    # Python 包和 CLI
  _C/                                # 项目自己的原生扩展
  language/                          # DSL 接口、类型与语义
  compiler/                          # AST 编译、源码诊断
  runtime/                           # kernel 声明与编译入口
  backends/                          # FlagQuantum 接入
python/src/                          # C++ IR builder 绑定
cmake/llvm-hash.txt                   # 已验证的 LLVM revision
test/                                # lit/FileCheck 与 Python 测试
docs/architecture.md
```

## Quanta dialect 设计

- 量子与经典表示共存；经典函数、算术与控制流复用标准 dialect。

| 类型 | 语义 | 状态 |
|---|---|---|
| `!quanta.qubit` | 单个逻辑量子比特的引用，无类型参数 | 已实现 |
| `!quanta.qreg<N>` | N 个量子比特引用组成的有序集合 | 暂缓，按实际需求引入 |
| `!quanta.qubit_value`，名称待定 | 优化阶段使用的线性量子值，操作消费旧值、返回新值 | 待设计 |

当前用多个 `!quanta.qubit` 表达多比特接口。该类型可以作为函数参数和返回值；
传递引用不复制量子态，也不表示一次量子状态更新。门通过副作用更新引用所指的
量子态，不返回新的 qubit。引用可以由 `quanta.alloc` 创建，也可以由调用方借给函数。

类型由 TableGen 定义，支持 MLIR 文本与字节码往返。
目前支持 IR 的解析、校验和打印，尚未接入执行后端。

### 量子门

首批门共用 `Quanta_GateOp` 定义，采用与 CUDA-Q Quake 相同的门修饰方式：

| 字段 | 表示 | 约定 |
|---|---|---|
| `parameters` | `f64` SSA 操作数列表 | 旋转角度以弧度为单位，可来自运行时计算 |
| `controls` | `!quanta.qubit` SSA 操作数列表 | 零个或多个正控制位，仅在所有控制位为 `\|1⟩` 的子空间施加门 |
| `targets` | `!quanta.qubit` SSA 操作数列表 | 基础门的目标位，数量由各门限定 |
| `is_adj` | 可选的 `UnitAttr` | 缺省施加 U；存在时施加 U†，文本写作 `<adj>` |

控制操作保持量子相干性，不隐含测量。参数、控制位和目标位是三个操作数分段，
由 `AttrSizedOperandSegments` 记录边界。引用形式的门均无 SSA 返回值。

| op | 参数数 | 目标数 | 基础门语义 |
|---|---|---|---|
| `quanta.h` | 0 | 1 | Hadamard，(X + Z) / √2 |
| `quanta.x`、`quanta.y`、`quanta.z` | 0 | 1 | Pauli X、Y、Z |
| `quanta.s` | 0 | 1 | diag(1, i) |
| `quanta.t` | 0 | 1 | diag(1, exp(iπ/4)) |
| `quanta.rx`、`quanta.ry`、`quanta.rz` | 1 | 1 | exp(−iθP/2)，P 分别为 X、Y、Z |
| `quanta.swap` | 0 | 2 | 交换两个目标位的量子态 |

例如，CX 用一个控制位的 `quanta.x` 表示，CCX 用两个控制位；S† 用
`quanta.s<adj>` 表示。所有这些门均可组合控制位和 `adj`：

```mlir
quanta.h %q0 : (!quanta.qubit) -> ()
quanta.x [%q0] %q1 : (!quanta.qubit, !quanta.qubit) -> ()
quanta.rx<adj> (%theta) [%q0] %q1 : (f64, !quanta.qubit, !quanta.qubit) -> ()
quanta.s<adj> %q1 : (!quanta.qubit) -> ()
quanta.swap %q0, %q1 : (!quanta.qubit, !quanta.qubit) -> ()
```

`RX(−θ)` 加 `adj` 的语义等价于 `RX(θ)`。目前保留参数和标记，不执行这类
量子门化简；H、X 等自伴随门上的 `adj` 也会保留。

每个门的所有控制位与目标位必须指向不同的逻辑 qubit。校验器检查参数与目标数量、
操作数类型，并拒绝重复的 SSA qubit。不同 SSA 引用是否别名仍需调用方保证，
当前校验器不能证明这一点。

门通过 `MemoryEffectsOpInterface` 声明对默认资源的保守读写副作用，不绑定到
单个 SSA 引用，以避免引用别名导致错误优化。没有标记为 `Pure` 或可推测执行。
工具注册了标准 `canonicalize` 和 `cse` pass；测试确认门的顺序、重复门与修饰符
在这些 pass 后保留。更精细的量子资源、别名分析和门优化按后续需求引入。

暂不支持负控制、寄存器、线性量子值和后端 lowering。
多控制门在 IR 层可表达，目标后端的门分解仍待实现。

### 分配、测量与生命周期

以下操作独立于 `Quanta_GateOp`，不支持 `is_adj` 或量子控制位：

| op | 输入 → 输出 | 语义 | 保守副作用 |
|---|---|---|---|
| `quanta.alloc` | 无 → `!quanta.qubit` | 每次动态执行创建一个全新的、初态为 `\|0⟩` 的逻辑 qubit | Alloc、Write |
| `quanta.measure` | `!quanta.qubit` → `i1` | Z 基投影测量，返回单次测量结果，引用继续有效 | Read、Write |
| `quanta.reset` | `!quanta.qubit` → 无 | 重置为 `\|0⟩`，引用继续有效，不返回经典值 | Read、Write |
| `quanta.dealloc` | `!quanta.qubit` → 无 | 丢弃量子子系统并结束其生命周期，所有别名失效 | Free、Read、Write |

这些副作用与门一样作用于默认资源，不依赖 SSA 引用是否别名，也不允许推测执行。
通用 `canonicalize` 和 `cse` 不会因为结果无人使用而删除测量，也不会合并新分配的 qubit。

`measure` 结果 0 对应 `|0⟩`，1 对应 `|1⟩`，不隐含 reset 或释放；经典结果可直接供
`scf.if` 使用，且在 qubit 释放后依然有效。每次执行返回一个 bit，shots 属外层执行配置。
第一版固定 Z 基并返回 `i1`，不引入测量句柄、其他测量基或期望值。

`reset` 将目标置回 `|0⟩` 并解除其与其余系统的关联。`dealloc` 允许任意量子态，不要求
事先测量或 reset；后端若要求释放前重置，应在 lowering 处理。各操作的精确态语义见对应
op 的 `description`。

```mlir
func.func @random_bit() -> i1 {
  %q = quanta.alloc : !quanta.qubit
  quanta.h %q : (!quanta.qubit) -> ()
  %bit = quanta.measure %q : !quanta.qubit -> i1
  quanta.dealloc %q : !quanta.qubit
  return %bit : i1
}
```

生命周期约定：

- 分配方拥有 qubit，必须在每条执行路径上恰好释放一次。Python 前端会为局部分配插入释放；
  手写 IR 仍需显式释放，当前没有自动补充释放的 IR pass。
- 函数参数是借用引用；被调用函数可以施加门、测量、reset，但不能释放它。
- 当前不支持转移所有权。函数可以转发借用引用，但不能将新分配的拥有型引用逸出。
- `dealloc` 后，所有指向同一逻辑 qubit 的引用都失效；经典测量结果不受影响。

校验器检查操作数/结果的类型与数量、拒绝非酉操作上的 `is_adj`，并捕获直接释放借用参数、
以及同一基本块内对同一 SSA 引用的释放后使用（含重复释放）。

这不是完整的生命周期证明：别名、跨基本块或嵌套区域的释放与使用、遗漏释放和拥有型引用
逸出尚未检查，需后续生命周期分析。

## Python 前端组织

参考 Triton 的分层：`language/core.py` 提供语言接口与前端值，`language/semantic.py`
检查语义，`compiler/code_generator.py` 遍历 AST，`compiler/compiler.py` 组织编译，
`runtime/kernel.py` 管理装饰器入口。`@kernel` 保留 Python 函数，`.compile()` 分析源码，
通过原生 builder 构造并校验 MLIR，不执行内核体。每次编译创建独立 Module，当前不缓存。

```python
import flagtree_q as fq
import flagtree_q.language as ql

@fq.kernel
def circuit(theta: float) -> bool:
    q0 = ql.qubit()
    q1 = ql.qubit()
    ql.h(q0)
    ql.x.ctrl(q0, q1)
    ql.rx.inv(theta, q1)
    return ql.measure(q1)

module = circuit.compile()
print(module)
```

第一版支持单基本块直线程序：

- 参数必须标注 `float`、`int`、`bool` 或 `ql.qubit`，对应 `f64`、`i64`、`i1` 和借用引用。
  返回值为一个经典标量，或 `None`；省略返回标注表示无返回值。参数保持为运行时 SSA 值。
- 支持局部赋值、qubit 引用别名、标量常量、`+ - * /` 和一元 `+ -`；赋值不改变已有变量类型。
  `int` 是有符号 i64，字面量需在范围内，整数加减乘按 64 位回绕。混合算术与 `/` 转为 f64；
  门角度和浮点返回值也接受 int 到 f64 的转换。bool 不隐式参与算术或充当角度。
- 全部现有量子门、Z 基测量和 reset 可用。`.inv` 映射到 IR 的 `is_adj`，连续两次 `.inv`
  抵消；只有酉门支持 `.inv` 与 `.ctrl`。参数顺序是角度、控制位、目标位，例如
  `ql.rx.ctrl.inv(theta, [q0, q1], q2)`。控制位列表/元组只组织已有引用，不引入寄存器类型。
- 局部 `ql.qubit()` 在唯一出口前逆序释放，即使原变量被覆盖也会释放原分配。借用参数不释放，
  禁止返回 qubit；同一引用不能重复出现在一个门的控制位与目标位中。
- 返回语句必须位于末尾。暂不支持 if/循环、kernel 间调用、默认参数、任意 Python 函数调用
  或对象属性访问；未知语法明确报错。内核需有可获取的 Python 源码，标量全局变量和闭包常量
  在每次编译时读取；错误保留文件、行列及原始源码，包括缩进和 UTF-8 字符。

当前 `.compile()` 只生成 IR；直接调用 kernel 会报错，执行后端和 shots 接口尚未接入。

已实现 `python/src` 中的 pybind11 绑定，生成 `flagtree_q._C.libflagtree_q`。
Python 直接通过它调用 MLIR C++ builder；不依赖 MLIR 官方 Python bindings。
现阶段暴露 Context、Type、Module、Function、Value、Builder，支持单基本块函数、
常量、算术、现有 Quanta 操作、源码位置、校验和打印。`_C` 是内部编译接口；其 `adj` 参数
保留 IR 命名，公共 DSL 使用 `.inv`。

Value 和 Function 保留 Module，Module 和 Type 保留 Context，避免 Python 对象提前
销毁造成悬空句柄。当前不公开操作删除或修改 IR 的 passes；后续增加时需处理句柄失效。
构造时拒绝跨模块、跨函数或跨 Context 的误用，原生 verifier 的诊断转成 Python 异常。
绑定层显式释放 qubit，AST 前端负责插入释放。

LLVM/MLIR 独立构建，版本记录在 `cmake/llvm-hash.txt`，通过 `MLIR_DIR` 指向同一构建的
头文件、库和工具。该文件记录已验证版本，当前不自动下载或检查源码 revision。
现有工具和扩展复用 Quanta 库；仅扩展开启 C++ RTTI 与异常处理。

激活 `flagtree-q` 环境并设置 `MLIR_DIR` 后，`python -m pip install -e .` 会构建扩展。
已有本地构建依赖时可加 `--no-build-isolation`；无需启用 `MLIR_ENABLE_BINDINGS_PYTHON`。
直接使用 CMake 构建和测试：

```sh
cmake -S . -B build -G Ninja -DMLIR_DIR="$MLIR_DIR" \
  -DCMAKE_BUILD_TYPE=Release -DFLAGTREE_Q_BUILD_PYTHON_MODULE=ON \
  -DPython3_EXECUTABLE="$CONDA_PREFIX/bin/python"
cmake --build build --target check-flagtree-q -j4
```

原生扩展默认输出到源码包的 `_C/`，二进制不纳入 Git；wheel 构建会输出到打包目录。
`FLAGTREE_Q_BUILD_PYTHON_MODULE=OFF` 时仍可独立构建 C++ 工具和运行原有测试。
当前本机 wheel 仍依赖 LLVM 引入的 Homebrew `libzstd`，尚未处理跨机器发布时的动态库打包。
