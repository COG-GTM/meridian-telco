import json
import re
from decimal import Decimal
from pathlib import Path

from telco_billing_rules import (
    MB_PER_GB,
    OVERAGE_RATE_PER_GB,
    BILLING_MONTH_DAYS,
    federal_tax,
    money,
    overage_gb,
    provincial_tax,
    rate_overage,
    rates_for_province,
    usage_gb_rounded,
)
from telco_billing_rules.proration import prorated_plan_charge


ROOT = Path(__file__).parents[1]
RULES_PATH = ROOT / "telco_billing_rules" / "billing_rules.json"
HEADER_PATH = ROOT / "cpp" / "telco_billing_rules.h"


def test_proration():
    assert prorated_plan_charge(640, 0, 0) == Decimal("640.00")
    assert prorated_plan_charge(300, 600, 16) == Decimal("450.00")
    assert prorated_plan_charge(300, 600, 31) == Decimal("600.00")
    assert prorated_plan_charge(300, 600, 1) == Decimal("300.00")
    assert prorated_plan_charge(300, 600, -1) == Decimal("300.00")


def test_rating():
    assert usage_gb_rounded(1024) == 1
    assert usage_gb_rounded(1025) == 2
    assert usage_gb_rounded(0) == 0
    assert overage_gb(2000 * 1024 + 1000, 2000) == 1
    assert rate_overage(2000 * 1024 + 1000, 2000) == Decimal("10.00")
    assert rate_overage(2000 * 1024 + 1, 2000) == Decimal("10.00")
    assert rate_overage(2000 * 1024, 2000) == Decimal("0.00")


def test_tax():
    bc = rates_for_province("BC")
    assert federal_tax(100, bc) == Decimal("5.00")
    assert provincial_tax(100, 10, bc) == Decimal("6.30")
    on = rates_for_province("ON")
    assert federal_tax(100, on) == Decimal("13.00")
    assert provincial_tax(100, 10, on) == Decimal("0.00")
    assert provincial_tax(1000, 100, rates_for_province("QC")) == Decimal("89.78")
    assert provincial_tax(100, 0, rates_for_province("AB")) == Decimal("0.00")
    for province in ("XX", "", "bc"):
        rates = rates_for_province(province)
        assert rates.federal_pct == Decimal("5")
        assert rates.federal_label == "GST"
        assert rates.provincial_pct == Decimal("0")


def test_header_matches_json():
    rules = json.loads(RULES_PATH.read_text())
    header = HEADER_PATH.read_text()
    assert int(re.search(r"BILLING_MONTH_DAYS = (\d+)", header).group(1)) == rules["proration"]["billing_month_days"]
    assert int(re.search(r"MB_PER_GB = (\d+)", header).group(1)) == rules["rating"]["mb_per_gb"]
    assert float(re.search(r"OVERAGE_RATE_PER_GB = ([0-9.]+)", header).group(1)) == rules["rating"]["overage_rate_per_gb"]
    for province, expected in rules["tax"]["provinces"].items():
        match = re.search(
            rf'province == "{province}"\) return make\(([^;]+)\);',
            header,
        )
        assert match
        values = [value.strip() for value in match.group(1).split(",")]
        assert Decimal(values[0]) == Decimal(str(expected["federal_pct"]))
        assert Decimal(values[1]) == Decimal(str(expected["provincial_pct"]))
        assert values[2].strip('"') == expected["federal_label"]
        assert values[3].strip('"') == expected["provincial_label"]


def test_exports_follow_source_of_truth():
    assert BILLING_MONTH_DAYS == 30
    assert MB_PER_GB == 1024
    assert OVERAGE_RATE_PER_GB == Decimal("10.0")
    assert money(1.005) == Decimal("1.01")
