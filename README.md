# FlagTree-Q

FlagTree-Q 是基于 MLIR 的量子与经典混合程序编译项目。

```python
import flagtree_q as fq
import flagtree_q.language as ql


@fq.kernel
def random_bit() -> bool:
    q = ql.qubit()
    ql.h(q)
    return ql.measure(q)


print(random_bit.compile())
```
