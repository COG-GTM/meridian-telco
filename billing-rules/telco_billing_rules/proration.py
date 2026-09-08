from __future__ import annotations

from decimal import Decimal

from .money import money
from .rules import RULES

BILLING_MONTH_DAYS = RULES.billing_month_days


def _decimal(value: int | float | Decimal) -> Decimal:
    return Decimal(str(value))


def daily_rate(monthly_fee: int | float | Decimal) -> Decimal:
    return _decimal(monthly_fee) / Decimal(BILLING_MONTH_DAYS)


def prorated_plan_charge(
    monthly_fee: int | float | Decimal,
    prev_monthly_fee: int | float | Decimal,
    change_day: int,
) -> Decimal:
    fee = _decimal(monthly_fee)
    previous_fee = _decimal(prev_monthly_fee)
    if change_day <= 0:
        return money(fee)
    days_on_old = max(0, min(int(change_day) - 1, BILLING_MONTH_DAYS))
    days_on_new = BILLING_MONTH_DAYS - days_on_old
    old_part = money(daily_rate(previous_fee) * days_on_old)
    new_part = money(daily_rate(fee) * days_on_new)
    return money(old_part + new_part)
