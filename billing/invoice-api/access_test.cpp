#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include "access.h"

/* invoice-api access checks. bin/invoice-api-test, run with make check. */

static int failures = 0;

static void check_true(const char *what, bool cond) {
  if (!cond) {
    printf("FAIL %s\n", what);
    failures++;
  } else {
    printf("ok   %s\n", what);
  }
}

static const char *FINANCE_TOKEN = "f1n4nce-0123456789abcdef0123456789abcdef";
static const char *BEACON_TOKEN = "b3acon-0123456789abcdef0123456789abcdef";

static std::string keys_text() {
  return std::string("# caller token scope\n") +
         "finance-recon " + FINANCE_TOKEN + " *\n" +
         "beacon-portal " + BEACON_TOKEN + " BEACON-004417,MER-011000  # two accounts\n";
}

int main() {
  std::vector<ApiCaller> callers;
  std::string err;
  check_true("keys file parses", parse_api_keys(keys_text(), &callers, &err) && callers.size() == 2);

  /* authentication */
  check_true("no authorization header is rejected", authenticate(callers, "") == 0);
  check_true("wrong token is rejected",
             authenticate(callers, "Bearer f1n4nce-0123456789abcdef0123456789abcdee") == 0);
  check_true("token without Bearer scheme is rejected", authenticate(callers, FINANCE_TOKEN) == 0);
  check_true("token prefix is rejected", authenticate(callers, "Bearer f1n4nce-0123") == 0);
  const ApiCaller *fin = authenticate(callers, std::string("Bearer ") + FINANCE_TOKEN);
  const ApiCaller *bea = authenticate(callers, std::string("Bearer ") + BEACON_TOKEN);
  check_true("finance token resolves to finance-recon", fin && fin->id == "finance-recon");
  check_true("beacon token resolves to beacon-portal", bea && bea->id == "beacon-portal");

  /* authorization */
  check_true("wildcard caller reads any account", fin && caller_may_read(*fin, "MER-011420"));
  check_true("scoped caller reads its own account", bea && caller_may_read(*bea, "MER-011000"));
  check_true("scoped caller cannot read another account", bea && !caller_may_read(*bea, "MER-011007"));
  check_true("scope match is exact", bea && !caller_may_read(*bea, "BEACON-00441"));

  /* keys file validation */
  check_true("empty keys file is refused", !parse_api_keys("# nothing\n", &callers, &err));
  check_true("short token is refused", !parse_api_keys("svc short *\n", &callers, &err));
  check_true("missing scope is refused",
             !parse_api_keys(std::string("svc ") + FINANCE_TOKEN + "\n", &callers, &err));
  check_true("duplicate token is refused",
             !parse_api_keys(std::string("a ") + FINANCE_TOKEN + " *\nb " + FINANCE_TOKEN + " *\n",
                             &callers, &err));

  char dir[] = "/tmp/invoice-api-test-XXXXXX";
  if (!mkdtemp(dir)) {
    printf("FAIL cannot create temp dir\n");
    return 1;
  }
  std::string keys = std::string(dir) + "/keys";
  FILE *f = fopen(keys.c_str(), "w");
  fputs(keys_text().c_str(), f);
  fclose(f);
  chmod(keys.c_str(), 0644);
  check_true("group/world readable keys file is refused", !load_api_keys(keys, &callers, &err));
  chmod(keys.c_str(), 0600);
  check_true("0600 keys file loads", load_api_keys(keys, &callers, &err) && callers.size() == 2);
  check_true("missing keys file is refused", !load_api_keys(std::string(dir) + "/nope", &callers, &err));

  /* audit */
  AuditRecord r;
  r.ts = "2026-07-31T00:00:00Z";
  r.caller = "beacon-portal";
  r.peer = "10.20.4.7";
  r.method = "GET";
  r.target = "/invoices?acct=\"x\n{\"forged\":1}";
  r.status = 200;
  r.disclosed.push_back("BEACON-004417/2026-07");
  std::string line = audit_line(r);
  check_true("audit record is a single line", line.find('\n') == line.size() - 1);
  check_true("audit record names caller and disclosure",
             line.find("\"caller\":\"beacon-portal\"") != std::string::npos &&
             line.find("\"disclosed\":[\"BEACON-004417/2026-07\"]") != std::string::npos);
  check_true("audit record escapes request target",
             line.find("\\\"x\\u000a{\\\"forged\\\":1}") != std::string::npos);

  std::string log = std::string(dir) + "/audit.log";
  check_true("audit record appends", append_audit(log, r) && append_audit(log, r));
  struct stat st;
  check_true("audit log is created 0600", stat(log.c_str(), &st) == 0 && (st.st_mode & 0777) == 0600);
  check_true("unwritable audit path fails", !append_audit(std::string(dir) + "/no/such/dir", r));

  unlink(keys.c_str());
  unlink(log.c_str());
  rmdir(dir);

  if (failures) {
    printf("\n%d check(s) failed\n", failures);
    return 1;
  }
  printf("\nall checks passed\n");
  return 0;
}
