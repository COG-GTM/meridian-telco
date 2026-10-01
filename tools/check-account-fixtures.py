#!/usr/bin/env python3
"""Fail if account data committed to the repo is not marked synthetic.

Everything under billing/accounts/ and data/accounts.csv is a fixture. Each
record must carry DATA_CLASS=SYNTHETIC and a TAX_ID in the 00- range, which is
never issued. Live customer records stay outside the repo (see README).
"""

import csv
import re
import subprocess
import sys

TAX_ID = re.compile(r"^00-\d{7}$")


def tracked(*paths):
    out = subprocess.run(["git", "ls-files", "--", *paths], check=True,
                         capture_output=True, text=True).stdout
    return [p for p in out.splitlines() if p]


def check(where, record, errors):
    if record.get("DATA_CLASS") != "SYNTHETIC":
        errors.append(f"{where}: DATA_CLASS is not SYNTHETIC")
    if not TAX_ID.match(record.get("TAX_ID", "")):
        errors.append(f"{where}: TAX_ID is not a synthetic 00-nnnnnnn value")


def main():
    errors = []
    for path in tracked("billing/accounts"):
        if not path.endswith(".rec"):
            continue
        with open(path) as f:
            record = dict(line.rstrip("\r\n").split("=", 1) for line in f if "=" in line)
        check(path, record, errors)
    for path in tracked("data/accounts.csv"):
        with open(path, newline="") as f:
            for n, row in enumerate(csv.DictReader(f), start=2):
                check(f"{path}:{n}", row, errors)
    for e in errors:
        print(e)
    if errors:
        print(f"{len(errors)} account fixture problem(s); live customer data must not be committed")
        return 1
    print("account fixtures ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
