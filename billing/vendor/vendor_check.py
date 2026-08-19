#!/usr/bin/env python3
"""Verify a vendored copy of telco-billing-rules against its VENDOR_STAMP.

This file is copied into the vendored directory itself, so consumer repos can
run it in CI without a checkout of the library:

    python <vendor-dir>/vendor_check.py

It fails when a vendored file has been edited in place. Rules changes belong in
COG-GTM/telco-billing-rules, followed by a re-vendor.
"""

from __future__ import annotations

import hashlib
import sys
from pathlib import Path

VENDOR_DIR = Path(__file__).resolve().parent
STAMP = VENDOR_DIR / "VENDOR_STAMP"


def main() -> int:
    if not STAMP.exists():
        print(f"no VENDOR_STAMP next to {Path(__file__).name}")
        return 1
    problems = []
    checked = 0
    for line in STAMP.read_text().splitlines():
        if not line.strip() or line.startswith("#") or line.startswith("source "):
            continue
        sha, relative = line.split("  ", 1)
        path = VENDOR_DIR / relative
        if not path.exists():
            problems.append(f"missing {relative}")
            continue
        if hashlib.sha256(path.read_bytes()).hexdigest() != sha:
            problems.append(f"locally modified {relative}")
        checked += 1
    for problem in problems:
        print(problem)
    if problems:
        print("edit COG-GTM/telco-billing-rules and re-vendor instead of patching here")
        return 1
    print(f"vendored billing rules clean ({checked} files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
