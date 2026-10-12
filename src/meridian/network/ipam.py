"""Entry point replacing bin/ipam (network/ipam.cpp). Not ported yet."""

from __future__ import annotations

from collections.abc import Sequence

from meridian.core.cli import argv_or_default, not_ported, parse_args

PROG = "meridian-ipam"
DEFAULTS: dict[str, str] = {"--devices": "network/devices.csv"}
FLAGS: tuple[str, ...] = ()


def parse(argv: Sequence[str]) -> dict[str, str | bool]:
    return parse_args(argv, DEFAULTS, FLAGS)


def main(argv: Sequence[str] | None = None) -> int:
    parse(argv_or_default(argv))
    return not_ported(PROG)
