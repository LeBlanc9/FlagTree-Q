# Conversion（预留）

后续以 `QuantaToFlagQuantum/` 为首个目标转换目录，并在
`include/FlagTreeQ/Conversion/` 中添加对应公开接口。

这里负责把经过校验的真实 MLIR 转换到双方约定的 FlagQuantum 接口。
当前未定义该接口、未实现转换，也不依赖 FlagQuantum。
真机目标的 lowering 作为后续独立方向。
