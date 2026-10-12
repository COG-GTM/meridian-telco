"""Argument handling shared by the console entry points.

Mirrors the hand-rolled argv loops in the C++ mains: options match exactly, a
value option given as the last argument (no value after it) is ignored, and
unknown arguments are skipped rather than rejected.
"""

from __future__ import annotations

import sys
from collections.abc import Iterable, Mapping, Sequence

NOT_PORTED_EXIT = 2


def parse_args(
    argv: Sequence[str],
    value_opts: Mapping[str, str],
    flag_opts: Iterable[str] = (),
) -> dict[str, str | bool]:
    flags = tuple(flag_opts)
    opts: dict[str, str | bool] = dict(value_opts)
    opts.update(dict.fromkeys(flags, False))
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in value_opts and i + 1 < len(argv):
            i += 1
            opts[a] = argv[i]
        elif a in flags:
            opts[a] = True
        i += 1
    return opts


def c_atoi(s: str) -> int:
    """C atoi(): skip leading whitespace, optional sign, then digits; 0 if none."""
    s = s.lstrip(" \t\n\v\f\r")
    sign = 1
    if s[:1] in ("+", "-"):
        sign = -1 if s[0] == "-" else 1
        s = s[1:]
    n = 0
    for ch in s:
        if not "0" <= ch <= "9":
            break
        n = n * 10 + ord(ch) - ord("0")
    return sign * n


def not_ported(prog: str) -> int:
    print(f"{prog}: not ported yet", file=sys.stderr)
    return NOT_PORTED_EXIT


def argv_or_default(argv: Sequence[str] | None) -> Sequence[str]:
    return sys.argv[1:] if argv is None else argv
