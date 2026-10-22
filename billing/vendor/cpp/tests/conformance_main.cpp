#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "billing_rules/rules.h"

/* The C++ binding must reproduce the meridian expectations in
   conformance/vectors.tsv, the same file the Python binding is checked against. */

static std::vector<std::string> split(const std::string &line, char sep) {
  std::vector<std::string> out;
  std::string field;
  for (size_t i = 0; i < line.size(); i++) {
    if (line[i] == sep) {
      out.push_back(field);
      field.clear();
    } else {
      field += line[i];
    }
  }
  out.push_back(field);
  return out;
}

static int failures = 0;

static void check_eq(const std::string &name, const char *what, double got, double want) {
  if (fabs(got - want) > 0.0049) {
    printf("FAIL %-24s %-18s got %.4f want %.4f\n", name.c_str(), what, got, want);
    failures++;
  }
}

int main(int argc, char **argv) {
  std::string path = argc > 1 ? argv[1] : "conformance/vectors.tsv";
  std::ifstream f(path.c_str());
  if (!f) {
    printf("cannot open %s\n", path.c_str());
    return 2;
  }

  std::vector<std::string> header;
  std::string line;
  int cases = 0;
  while (std::getline(f, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::vector<std::string> fields = split(line, '|');
    if (header.empty()) {
      header = fields;
      continue;
    }
    std::map<std::string, std::string> row;
    for (size_t i = 0; i < header.size() && i < fields.size(); i++) row[header[i]] = fields[i];

    billing_rules::InvoiceInputs in;
    in.period = row["period"];
    in.province = row["province"];
    in.usage_mb = atol(row["usage_mb"].c_str());
    in.included_gb = atol(row["included_gb"].c_str());
    in.plan_fee = atof(row["plan_fee"].c_str());
    in.previous_plan_fee = atof(row["previous_plan_fee"].c_str());
    in.plan_change_day = atoi(row["plan_change_day"].c_str());
    in.line_count = atoi(row["line_count"].c_str());
    in.promo_amount = atof(row["promo_amount"].c_str());
    in.promo_issued_on = row["promo_issued_on"];
    in.suspension_start_day = atoi(row["suspension_start_day"].c_str());
    in.suspension_end_day = atoi(row["suspension_end_day"].c_str());
    in.prior_balance = atof(row["prior_balance"].c_str());
    in.prior_due_date = row["prior_due_date"];
    in.loyalty_pct = atof(row["loyalty_pct"].c_str());

    billing_rules::InvoiceAmounts got = billing_rules::compute_invoice_amounts(in);
    const std::string &name = row["name"];
    check_eq(name, "overage_gb", (double)got.overage_gb, atof(row["overage_gb"].c_str()));
    check_eq(name, "plan_charge", got.plan_charge, atof(row["plan_charge"].c_str()));
    check_eq(name, "line_discount", got.line_discount, atof(row["line_discount"].c_str()));
    check_eq(name, "overage_charges", got.overage_charges, atof(row["overage_charges"].c_str()));
    check_eq(name, "suspension_credit", got.suspension_credit, atof(row["suspension_credit"].c_str()));
    check_eq(name, "promo_credit", got.promo_credit, atof(row["promo_credit"].c_str()));
    check_eq(name, "late_fee", got.late_fee, atof(row["late_fee"].c_str()));
    check_eq(name, "subtotal", got.subtotal, atof(row["subtotal"].c_str()));
    check_eq(name, "loyalty_discount", got.loyalty_discount, atof(row["loyalty_discount"].c_str()));
    check_eq(name, "federal_tax", got.federal_tax, atof(row["federal_tax"].c_str()));
    check_eq(name, "provincial_tax", got.provincial_tax, atof(row["provincial_tax"].c_str()));
    check_eq(name, "total", got.total, atof(row["total"].c_str()));
    cases++;
  }

  if (failures) {
    printf("\n%d check(s) failed across %d vector(s)\n", failures, cases);
    return 1;
  }
  printf("all %d conformance vectors passed\n", cases);
  return 0;
}
