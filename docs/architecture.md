## 工程结构

```text
include/FlagTreeQ/Dialect/Quanta/IR/   # 公开头文件、TableGen 定义
lib/Dialect/Quanta/IR/                # dialect 实现
lib/Dialect/Quanta/Transforms/        # 量子分析与优化
lib/Conversion/                      # 目标转换
tools/flagtree-q-opt/                 # 原生 MLIR 工具
python/flagtree_q/                    # Python 包和 CLI
  backends/                          # FlagQuantum 接入
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

参考：[CUDA-Q Quake 门定义](https://github.com/NVIDIA/cuda-quantum/blob/main/cudaq/include/cudaq/Optimizer/Dialect/Quake/QuakeOps.td)。

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

`measure` 的结果 0 对应 `|0⟩`，1 对应 `|1⟩`。对联合状态 ρ，结果 b 的概率为
Tr(P_b ρ)，测量后的联合状态为 P_b ρ P_b / Tr(P_b ρ)，其中 P_b 仅投影目标 qubit。
测量不隐含 reset 或释放；后续门继续使用同一个引用，经典结果可以直接供 `scf.if`
使用，并且在 qubit 释放后依然有效。每次执行只返回一个 bit，shots 属于外层执行配置。

第一版固定使用 Z 基并直接返回 `i1`，不引入测量句柄、其他测量基或观测量期望值。

`reset` 清除目标 qubit 与其余系统的关联，同时保留其余系统的约化状态；其语义是
用 `|0⟩⟨0|` 与 Tr_qubit(ρ) 的张量积替换联合状态，按原有子系统顺序排列。
`dealloc` 允许任意量子态，不要求事先测量或 reset。其语义是丢弃该子系统，剩余系统
保留 Tr_qubit(ρ)，不产生经典测量结果。后端若要求释放前重置，应由 lowering 处理；
纠缠 qubit 的释放不能简单理解为删掉态向量的某个维度。

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

- 分配方拥有 qubit，必须在每条执行路径上恰好释放一次；当前不自动插入释放。
- 函数参数是借用引用；被调用函数可以施加门、测量、reset，但不能释放它。
- 当前不支持转移所有权。函数可以转发借用引用，但不能将新分配的拥有型引用逸出。
- `dealloc` 后，所有指向同一逻辑 qubit 的引用都失效；经典测量结果不受影响。

当前校验器检查操作数和结果的类型、数量，拒绝非酉操作上的 `is_adj`，并检查：

- 直接释放函数入口的借用参数，包括在嵌套区域中释放该参数。
- 同一基本块内对同一 SSA 引用的释放后使用，包括重复释放、继续施加门、测量、
  reset 和返回已释放的引用。

这不是完整的生命周期证明。不同 SSA 引用的别名、跨基本块或嵌套区域的释放与使用、
遗漏释放和拥有型引用逸出尚未检查，需要后续生命周期分析。互斥分支中分别释放同一个
拥有型引用是合法的，不能仅通过统计 `dealloc` 使用次数来判断重复释放。
