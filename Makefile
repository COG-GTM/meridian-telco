CXX = g++
VENDOR = billing/vendor
CXXFLAGS = -O2 -Wall -I$(VENDOR)/cpp/include
PYTHON ?= python3
LDFLAGS = -lsqlite3
BIN = bin

MEDIATION_OBJS = mediation/capacity.o mediation/status.o mediation/circuit_counter.o mediation/store.o mediation/locations.o
# the billing rules are vendored from COG-GTM/telco-billing-rules (see billing/vendor/VENDOR_STAMP)
RULES_OBJS = $(VENDOR)/cpp/src/rules.o
BILLING_OBJS = billing/accounts.o billing/invoice.o $(RULES_OBJS)

all: $(BIN)/mediation $(BIN)/inventory-api $(BIN)/billing-run $(BIN)/invoice-api $(BIN)/ipam

$(BIN):
	mkdir -p $(BIN)

$(BIN)/mediation: $(MEDIATION_OBJS) mediation/ingest.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN)/inventory-api: $(MEDIATION_OBJS) inventory-api/main.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN)/billing-run: $(BILLING_OBJS) billing/run_billing.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN)/invoice-api: $(BILLING_OBJS) billing/invoice-api/main.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN)/billing-test: $(BILLING_OBJS) billing/rules_test.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN)/rules-conformance: $(RULES_OBJS) $(VENDOR)/cpp/tests/conformance_main.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN)/ipam: network/ipam.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp $(wildcard $(VENDOR)/cpp/include/billing_rules/*.h)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

run: all
	$(BIN)/mediation --data data --db meridian.db

check: vendor-check $(BIN)/billing-test $(BIN)/rules-conformance
	$(BIN)/billing-test
	$(BIN)/rules-conformance $(VENDOR)/conformance/vectors.tsv

# fails if billing/vendor was patched in place instead of re-vendored
vendor-check:
	$(PYTHON) $(VENDOR)/vendor_check.py

register: all
	$(BIN)/billing-run --period 2026-07

demo: all
	@echo "invoice-api on :8082, invoice register page on :8083"
	@$(BIN)/invoice-api & \
	 python3 -m http.server 8083 --directory dashboard & \
	 wait

clean:
	rm -f $(BIN)/* */*.o */*/*.o $(VENDOR)/cpp/*/*.o meridian.db

.PHONY: all run check vendor-check register demo clean
