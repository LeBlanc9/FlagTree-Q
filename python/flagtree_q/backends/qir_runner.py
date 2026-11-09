"""Execute exported QIR with the optional qirrunner package."""

from collections import Counter
import re
from typing import Dict, Optional


def _runner_ir(qir: str) -> str:
    """Spell LLVM 23+ f64 hex constants for the runner's LLVM 20 parser."""
    # Both forms encode the same 64 bits. Skip quoted names/strings and comments;
    # replacing text globally could change a symbol or output label.
    tokens = (
        r'"(?:[^"\\]|\\.)*"|;[^\n]*|'
        r'(?<![\w.$@%!-])double(\s+)f(0x[0-9A-Fa-f]{16})(?![\w.$-])'
    )
    return re.sub(tokens, lambda m: "double" + m[1] + m[2] if m[1] else m[0], qir)


def _parse_counts(output: str, shots: int) -> Dict[str, int]:
    """Read successful shots using our scalar-result / empty-tuple schema."""
    counts = Counter()
    active = False
    result = None
    completed = 0
    for line in output.splitlines():
        fields = line.split("\t")
        if fields == ["START"] and not active:
            active = True
            result = None
        elif active and fields[0] == "METADATA":
            continue
        elif active and fields[0] == "OUTPUT" and result is None:
            if (len(fields) == 4 and fields[1] == "RESULT"
                    and fields[2] in ("0", "1") and fields[3] == "result"):
                result = fields[2]
            elif fields == ["OUTPUT", "TUPLE", "0", "unit"]:
                result = ""
            else:
                raise RuntimeError(f"unsupported QIR Runner output: {line!r}")
        elif active and fields == ["END", "0"] and result is not None:
            counts[result] += 1
            completed += 1
            active = False
        else:
            raise RuntimeError(f"invalid or failed QIR Runner shot: {line!r}")
    if active or completed != shots:
        raise RuntimeError(
            f"incomplete QIR Runner output: expected {shots} successful shots, "
            f"received {completed}"
        )
    return dict(sorted(counts.items()))


def sample_qir(qir: str, *, shots: int, seed: Optional[int]) -> Dict[str, int]:
    """Run FlagTree-Q's QIR output and count its returned measurement bits."""
    try:
        from qirrunner import OutputHandler, run_bytes
    except ModuleNotFoundError as error:
        if error.name != "qirrunner":
            raise
        raise ImportError(
            "the qir-runner backend requires qirrunner; install it with "
            "python -m pip install 'flagtree-q[qir-runner]' "
            "(or python -m pip install -e '.[qir-runner]' from the source tree)"
        ) from error

    output = OutputHandler()
    # Exchange textual LLVM IR, avoiding bitcode-version coupling with the runner.
    run_bytes(_runner_ir(qir).encode("utf-8"), shots=shots, rng_seed=seed,
              output_fn=output.handle)
    return _parse_counts(output.get_output(), shots)
