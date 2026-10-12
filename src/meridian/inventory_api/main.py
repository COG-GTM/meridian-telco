"""Entry point replacing bin/inventory-api (inventory-api/main.cpp). Not ported yet."""

from __future__ import annotations

from collections.abc import Sequence

from meridian.core.cli import argv_or_default, not_ported, parse_args

PROG = "meridian-inventory-api"
DEFAULTS: dict[str, str] = {
    "--db": "meridian.db",
    "--locations": "data/locations.csv",
    "--port": "8081",
}
FLAGS: tuple[str, ...] = ()


def parse(argv: Sequence[str]) -> dict[str, str | bool]:
    return parse_args(argv, DEFAULTS, FLAGS)


def main(argv: Sequence[str] | None = None) -> int:
    parse(argv_or_default(argv))
    return not_ported(PROG)
