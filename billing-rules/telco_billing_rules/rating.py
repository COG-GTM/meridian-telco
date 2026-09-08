from __future__ import annotations

from decimal import Decimal

from .money import money
from .rules import RULES

MB_PER_GB = RULES.mb_per_gb
OVERAGE_RATE_PER_GB = RULES.overage_rate_per_gb


def usage_gb_rounded(usage_mb: int) -> int:
    return (int(usage_mb) + MB_PER_GB - 1) // MB_PER_GB


def overage_gb(usage_mb: int, included_gb: int) -> int:
    return max(usage_gb_rounded(usage_mb) - int(included_gb), 0)


def rate_overage(usage_mb: int, included_gb: int) -> Decimal:
    return money(Decimal(overage_gb(usage_mb, included_gb)) * OVERAGE_RATE_PER_GB)
