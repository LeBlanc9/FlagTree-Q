# Conversion

`QuantaToQIR/` 将静态 Quanta 内核转换为 LLVM dialect，遵循 QIR 2.0 Base Profile。
共享的 QIR 函数声明、属性和模块标志位于 `Target/QIR`；LLVM IR 与 bitcode 导出也由该层完成。
Python 绑定与 CLI 串联 conversion 和 export；`Target` 不反向依赖这里的 pass。
支持范围和调用方式见 [架构文档](../../docs/architecture.md#qir-导出)。
