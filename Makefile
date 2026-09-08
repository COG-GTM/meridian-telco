CXX = g++
CXXFLAGS = -O2 -Wall
LDFLAGS = -lsqlite3
BIN = bin
.DEFAULT_GOAL := all
THIRD_PARTY = third_party/telco-capacity-rules
TELCO_CAPACITY_RULES_DIR ?= $(THIRD_PARTY)

MEDIATION_OBJS = mediation/capacity.o mediation/status.o mediation/circuit_counter.o mediation/store.o mediation/locations.o
BILLING_OBJS = billing/rating.o billing/discounts.o billing/accounts.o billing/tax.o \
               billing/proration.o billing/promo.o billing/suspension.o billing/lines.o \
               billing/latefee.o billing/invoice.o

CXXFLAGS += -I$(TELCO_CAPACITY_RULES_DIR)/cpp/include
TELCO_CAPACITY_HEADER = $(TELCO_CAPACITY_RULES_DIR)/cpp/include/telco_capacity/capacity.h

ifeq ($(TELCO_CAPACITY_RULES_DIR),$(THIRD_PARTY))
$(TELCO_CAPACITY_HEADER):
	mkdir -p $(THIRD_PARTY)
	git clone --depth 1 --branch v0.1.0 https://github.com/COG-GTM/telco-capacity-rules.git $(THIRD_PARTY)
endif

$(MEDIATION_OBJS): $(TELCO_CAPACITY_HEADER)

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

$(BIN)/ipam: network/ipam.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

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
	rm -f $(BIN)/* */*.o */*/*.o meridian.db

deps: $(TELCO_CAPACITY_HEADER)

.PHONY: all run check register demo clean deps
