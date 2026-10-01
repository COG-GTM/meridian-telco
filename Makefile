CXX = g++
CXXFLAGS = -O2 -Wall
LDFLAGS = -lsqlite3
BIN = bin

MEDIATION_OBJS = mediation/capacity.o mediation/status.o mediation/circuit_counter.o mediation/store.o mediation/locations.o
BILLING_OBJS = billing/rating.o billing/discounts.o billing/accounts.o billing/tax.o \
               billing/proration.o billing/promo.o billing/suspension.o billing/lines.o \
               billing/latefee.o billing/invoice.o

all: $(BIN)/mediation $(BIN)/inventory-api $(BIN)/billing-run $(BIN)/invoice-api $(BIN)/ipam

$(BIN):
	mkdir -p $(BIN)

$(BIN)/mediation: $(MEDIATION_OBJS) mediation/ingest.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN)/inventory-api: $(MEDIATION_OBJS) inventory-api/main.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN)/billing-run: $(BILLING_OBJS) billing/run_billing.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN)/invoice-api: $(BILLING_OBJS) billing/invoice-api/access.o billing/invoice-api/main.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN)/invoice-api-test: billing/invoice-api/access.o billing/invoice-api/access_test.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN)/billing-test: $(BILLING_OBJS) billing/rules_test.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN)/ipam: network/ipam.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

run: all
	$(BIN)/mediation --data data --db meridian.db

check: $(BIN)/billing-test $(BIN)/invoice-api-test
	$(BIN)/billing-test
	$(BIN)/invoice-api-test

register: all
	$(BIN)/billing-run --period 2026-07

INVOICE_API_KEYS ?= invoice-api.keys
INVOICE_API_AUDIT_LOG ?= invoice-api-audit.log

$(INVOICE_API_KEYS):
	@umask 077; printf 'register-demo %s *\n' "$$(od -An -tx1 -N24 /dev/urandom | tr -d ' \n')" > $@
	@echo "wrote demo invoice-api caller to $@"

demo: all $(INVOICE_API_KEYS)
	@echo "invoice-api on :8082, invoice register page on :8083"
	@echo "register page token: $$(awk '$$1 == "register-demo" { print $$2 }' $(INVOICE_API_KEYS))"
	@$(BIN)/invoice-api --keys $(INVOICE_API_KEYS) --audit-log $(INVOICE_API_AUDIT_LOG) & \
	 python3 -m http.server 8083 --directory dashboard & \
	 wait

clean:
	rm -f $(BIN)/* */*.o */*/*.o meridian.db

.PHONY: all run check register demo clean
