import importlib
import subprocess
import sys
from importlib.metadata import entry_points
from pathlib import Path

import pytest

from meridian.core.cli import NOT_PORTED_EXIT, c_atoi, parse_args

SCRIPTS = {
    "meridian-mediation": "meridian.mediation.ingest:main",
    "meridian-inventory-api": "meridian.inventory_api.main:main",
    "meridian-billing-run": "meridian.billing.run_billing:main",
    "meridian-invoice-api": "meridian.invoice_api.main:main",
    "meridian-billing-test": "meridian.billing.rules_check:main",
    "meridian-ipam": "meridian.network.ipam:main",
}

# Defaults and flags as hard-coded in each C++ main().
CPP_OPTIONS = {
    "meridian.mediation.ingest": ({"--data": "data", "--db": "meridian.db"}, ()),
    "meridian.inventory_api.main": (
        {"--db": "meridian.db", "--locations": "data/locations.csv", "--port": "8081"},
        (),
    ),
    "meridian.billing.run_billing": (
        {"--accounts": "billing/accounts", "--usage": "data/usage.csv", "--period": ""},
        ("--json",),
    ),
    "meridian.invoice_api.main": (
        {"--accounts": "billing/accounts", "--usage": "data/usage.csv", "--port": "8082"},
        (),
    ),
    "meridian.billing.rules_check": ({}, ()),
    "meridian.network.ipam": ({"--devices": "network/devices.csv"}, ()),
}


@pytest.mark.parametrize(
    "pkg",
    ["core", "mediation", "billing", "network", "inventory_api", "invoice_api"],
)
def test_subpackages_import(pkg):
    importlib.import_module(f"meridian.{pkg}")


def test_console_scripts_registered():
    eps = {ep.name: ep.value for ep in entry_points(group="console_scripts")}
    for name, target in SCRIPTS.items():
        assert eps.get(name) == target


@pytest.mark.parametrize("module", sorted(CPP_OPTIONS))
def test_cli_matches_cpp(module):
    mod = importlib.import_module(module)
    defaults, flags = CPP_OPTIONS[module]
    assert mod.DEFAULTS == defaults
    assert mod.FLAGS == flags


@pytest.mark.parametrize("name", sorted(SCRIPTS))
def test_stub_exits_not_ported(name, capsys):
    module, func = SCRIPTS[name].split(":")
    main = getattr(importlib.import_module(module), func)
    assert main([]) == NOT_PORTED_EXIT
    assert capsys.readouterr().err == f"{name}: not ported yet\n"


@pytest.mark.parametrize("name", sorted(SCRIPTS))
def test_installed_script_runs(name):
    script = Path(sys.executable).parent / name
    if not script.exists():
        pytest.skip(f"{name} not installed next to {sys.executable}")
    res = subprocess.run([str(script), "--json"], capture_output=True, text=True)
    assert res.returncode == NOT_PORTED_EXIT
    assert "not ported yet" in res.stderr


def test_billing_run_flags():
    from meridian.billing.run_billing import parse

    assert parse([]) == {
        "--accounts": "billing/accounts",
        "--usage": "data/usage.csv",
        "--period": "",
        "--json": False,
    }
    got = parse(["--period", "2026-07", "--json", "--accounts", "x"])
    assert got["--period"] == "2026-07"
    assert got["--json"] is True
    assert got["--accounts"] == "x"


def test_parse_args_matches_cpp_loop():
    opts = {"--db": "meridian.db", "--data": "data"}
    # unknown args are skipped, a trailing value option without a value is ignored
    assert parse_args(["--bogus", "--db", "x.db", "--data"], opts) == {
        "--db": "x.db",
        "--data": "data",
    }
    # the value after an option is consumed even if it looks like an option
    assert parse_args(["--db", "--data", "d"], opts) == {"--db": "--data", "--data": "data"}


@pytest.mark.parametrize(
    "s,n",
    [("8081", 8081), ("  42x", 42), ("-7", -7), ("+3", 3), ("abc", 0), ("", 0)],
)
def test_c_atoi(s, n):
    assert c_atoi(s) == n
