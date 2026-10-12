"""Entry point replacing bin/invoice-api (billing/invoice-api/main.cpp). Not ported yet."""

from __future__ import annotations

from collections.abc import Sequence

from meridian.core.cli import argv_or_default, not_ported, parse_args

PROG = "meridian-invoice-api"
DEFAULTS: dict[str, str] = {
    "--accounts": "billing/accounts",
    "--usage": "data/usage.csv",
    "--port": "8082",
}
FLAGS: tuple[str, ...] = ()


def parse(argv: Sequence[str]) -> dict[str, str | bool]:
    return parse_args(argv, DEFAULTS, FLAGS)


def main(argv: Sequence[str] | None = None) -> int:
    parse(argv_or_default(argv))
    return not_ported(PROG)
