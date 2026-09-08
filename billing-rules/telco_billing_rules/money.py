from __future__ import annotations

from decimal import Decimal, ROUND_HALF_UP


def money(amount: int | float | Decimal) -> Decimal:
    return Decimal(str(amount)).quantize(Decimal("0.01"), rounding=ROUND_HALF_UP)
