## 工程结构

```text
include/FlagTreeQ/Dialect/Quanta/IR/   # 公开头文件、TableGen 定义
lib/Dialect/Quanta/IR/                # dialect 实现
lib/Dialect/Quanta/Transforms/        # 量子分析与优化
lib/Conversion/                      # 目标转换
tools/flagtree-q-opt/                 # 原生 MLIR 工具
python/flagtree_q/                    # Python 包和 CLI
  frontend/                          # DSL 与 IR 生成
  backends/                          # FlagQuantum 接入
test/                                # lit/FileCheck 与 Python 测试
examples/                            # 当前可运行的 MLIR 示例
docs/architecture.md
```

## Quanta dialect 设计

- 量子与经典表示共存；经典函数、算术与控制流复用标准 dialect。

| 类型 | 语义 |
|---|---|
| `!quanta.qubit` | 单个逻辑量子比特的引用 |
| `!quanta.qreg<N>` | N 个量子比特引用组成的有序集合 |
| `!quanta.qubit_value`，名称待定 | 优化阶段使用的线性量子值，操作消费旧值、返回新值 |
