CXX = g++
CXXFLAGS = -O2 -Wall
LDFLAGS = -lsqlite3
BIN = bin

# shared business rules, git submodule (git submodule update --init)
TELCO_RULES_DIR = third_party/telco-rules
include $(TELCO_RULES_DIR)/telco-rules.mk
CPPFLAGS = $(TELCO_RULES_CPPFLAGS)

MEDIATION_OBJS = mediation/circuit_counter.o mediation/store.o mediation/locations.o $(TELCO_RULES_OBJS)
BILLING_OBJS = billing/accounts.o billing/invoice.o $(TELCO_RULES_OBJS)

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

$(BIN)/ipam: network/ipam.o $(TELCO_RULES_OBJS) | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

run: all
	$(BIN)/mediation --data data --db meridian.db

check: $(BIN)/billing-test
	$(BIN)/billing-test

register: all
	$(BIN)/billing-run --period 2026-07

demo: all
	@echo "invoice-api on :8082, invoice register page on :8083"
	@$(BIN)/invoice-api & \
	 python3 -m http.server 8083 --directory dashboard & \
	 wait

clean:
	rm -f $(BIN)/* */*.o */*/*.o $(TELCO_RULES_OBJS) meridian.db

.PHONY: all run check register demo clean
