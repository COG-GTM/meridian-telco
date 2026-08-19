CXX = g++
CXXFLAGS = -O2 -Wall -Ibilling/vendor/cpp/include
LDFLAGS = -lsqlite3
BIN = bin

MEDIATION_OBJS = mediation/capacity.o mediation/status.o mediation/circuit_counter.o mediation/store.o mediation/locations.o
# every billing rule comes from the shared library vendored in billing/vendor
# (COG-GTM/telco-billing-rules); billing/invoice.cpp only maps our records onto it
BILLING_OBJS = billing/vendor/cpp/src/rules.o billing/accounts.o billing/invoice.o

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

$(BIN)/billing-conformance: billing/vendor/cpp/src/rules.o billing/vendor/cpp/tests/conformance_main.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN)/ipam: network/ipam.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

run: all
	$(BIN)/mediation --data data --db meridian.db

check: $(BIN)/billing-test $(BIN)/billing-conformance
	python3 billing/vendor/vendor_check.py
	$(BIN)/billing-test
	$(BIN)/billing-conformance billing/vendor/conformance/vectors.tsv

register: all
	$(BIN)/billing-run --period 2026-07

demo: all
	@echo "invoice-api on :8082, invoice register page on :8083"
	@$(BIN)/invoice-api & \
	 python3 -m http.server 8083 --directory dashboard & \
	 wait

clean:
	rm -f $(BIN)/* meridian.db
	find . -name '*.o' -delete

.PHONY: all run check register demo clean
