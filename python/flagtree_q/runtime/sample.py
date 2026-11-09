"""Compile and sample a closed quantum kernel."""

from typing import Dict, Optional

from .kernel import Kernel


def sample(kernel: Kernel, *, shots: int = 1000, backend: str = "qir-runner",
           seed: Optional[int] = None) -> Dict[str, int]:
    """Return counts for the kernel's returned bit (or ``""`` for void).

    The kernel must satisfy the QIR export restrictions: no parameters, constant
    angles, and a returned measurement bit or void. Each shot starts in |0...0>.
    Only returned measurements contribute to counts; there is no implicit
    measurement. ``seed`` makes sampling reproducible for a fixed backend version.
    """
    if not isinstance(kernel, Kernel):
        raise TypeError("sample() requires an @kernel function")
    if type(shots) is not int:
        raise TypeError("shots must be an integer")
    if not 1 <= shots <= 2**32 - 1:
        raise ValueError("shots must be between 1 and 2**32 - 1")
    if seed is not None:
        if type(seed) is not int:
            raise TypeError("seed must be an integer or None")
        if not 0 <= seed <= 2**64 - 1:
            raise ValueError("seed must be between 0 and 2**64 - 1")
    if backend != "qir-runner":
        raise ValueError(f"unsupported sampling backend: {backend!r}")

    from ..backends.qir_runner import sample_qir

    qir = kernel.compile().to_qir()
    return sample_qir(qir, shots=shots, seed=seed)
