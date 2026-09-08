from __future__ import annotations

from dataclasses import dataclass
from decimal import Decimal

from .money import money
from .rules import RULES, TaxRule


TaxRates = TaxRule


def rates_for_province(province: str) -> TaxRates:
    return RULES.provinces.get(province, RULES.default_tax)


def federal_tax(pre_discount_amount: int | float | Decimal, rates: TaxRates) -> Decimal:
    return money(Decimal(str(pre_discount_amount)) * rates.federal_pct / Decimal(100))


def provincial_tax(
    pre_discount_amount: int | float | Decimal,
    discount: int | float | Decimal,
    rates: TaxRates,
) -> Decimal:
    if rates.provincial_pct <= 0:
        return Decimal("0.00")
    base = max(Decimal(str(pre_discount_amount)) - Decimal(str(discount)), Decimal("0"))
    return money(base * rates.provincial_pct / Decimal(100))
