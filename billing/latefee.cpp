#include "latefee.h"
#include "datecalc.h"
#include "money.h"

/* late fee. ten day grace after the due date, then 1.5% of the balance still
   outstanding when the next cycle opens. */

long days_past_due(std::string_view due_date, std::string_view period) {
  BillDate due = parse_date(due_date);
  BillDate cycle = period_start(period);
  if (!due.ok || !cycle.ok) return 0;
  long days = days_between(due, cycle);
  return days > 0 ? days : 0;
}

double late_fee(double prior_balance, std::string_view due_date, std::string_view period) {
  if (prior_balance <= 0.0) return 0.0;
  if (days_past_due(due_date, period) <= LATE_FEE_GRACE_DAYS) return 0.0;
  return money(prior_balance * LATE_FEE_PCT / 100.0);
}
