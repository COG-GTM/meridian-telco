from __future__ import annotations

import json
from dataclasses import dataclass
from decimal import Decimal
from importlib import resources
from typing import Any


@dataclass(frozen=True)
class TaxRates:
    federal_pct: Decimal
    provincial_pct: Decimal
    federal_label: str
    provincial_label: str


@dataclass(frozen=True)
class Rules:
    version: int
    rounding_mode: str
    rounding_decimals: int
    billing_month_days: int
    usage_unit: str
    mb_per_gb: int
    overage_rate_per_gb: Decimal
    federal_base: str
    provincial_base: str
    provinces: dict[str, TaxRates]
    default_tax: TaxRates


def _decimal(value: Any) -> Decimal:
    return Decimal(str(value))


def load_rules() -> Rules:
    text = resources.files("telco_billing_rules").joinpath("billing_rules.json").read_text()
    raw = json.loads(text)
    if raw["rounding"]["mode"] != "per_line" or raw["rounding"]["decimals"] != 2:
        raise ValueError("unsupported rounding policy")
    if raw["rating"]["usage_unit"] != "gb_round_up":
        raise ValueError("unsupported usage policy")
    if raw["tax"]["federal_base"] != "pre_loyalty_discount":
        raise ValueError("unsupported federal tax base")
    if raw["tax"]["provincial_base"] != "post_loyalty_discount":
        raise ValueError("unsupported provincial tax base")

    def tax_rule(value: dict[str, Any]) -> TaxRates:
        return TaxRates(
            federal_pct=_decimal(value["federal_pct"]),
            provincial_pct=_decimal(value["provincial_pct"]),
            federal_label=value["federal_label"],
            provincial_label=value["provincial_label"],
        )

    return Rules(
        version=int(raw["version"]),
        rounding_mode=raw["rounding"]["mode"],
        rounding_decimals=int(raw["rounding"]["decimals"]),
        billing_month_days=int(raw["proration"]["billing_month_days"]),
        usage_unit=raw["rating"]["usage_unit"],
        mb_per_gb=int(raw["rating"]["mb_per_gb"]),
        overage_rate_per_gb=_decimal(raw["rating"]["overage_rate_per_gb"]),
        federal_base=raw["tax"]["federal_base"],
        provincial_base=raw["tax"]["provincial_base"],
        provinces={key: tax_rule(value) for key, value in raw["tax"]["provinces"].items()},
        default_tax=tax_rule(raw["tax"]["default"]),
    )


RULES = load_rules()
