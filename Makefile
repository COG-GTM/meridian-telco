CAPACITY_LIB = vendor/capacity-lib
CXX = g++
CXXFLAGS = -O2 -Wall -I$(CAPACITY_LIB)/include
LDFLAGS = -lsqlite3
BIN = bin

MEDIATION_OBJS = mediation/capacity_lib.o mediation/capacity.o mediation/status.o mediation/circuit_counter.o mediation/store.o mediation/locations.o
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

$(BIN)/invoice-api: $(BILLING_OBJS) billing/invoice-api/main.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN)/billing-test: $(BILLING_OBJS) billing/rules_test.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN)/ipam: network/ipam.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN)/capacity-test: $(MEDIATION_OBJS) mediation/capacity_test.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# shared capacity rule, vendored as a git submodule
$(CAPACITY_LIB)/src/capacity.cpp:
	@echo "vendor/capacity-lib is missing: run git submodule update --init" && exit 1

mediation/capacity_lib.o: $(CAPACITY_LIB)/src/capacity.cpp $(CAPACITY_LIB)/include/capacity_lib/capacity.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

run: all
	$(BIN)/mediation --data data --db meridian.db

check: $(BIN)/billing-test $(BIN)/capacity-test
	$(BIN)/billing-test
	$(BIN)/capacity-test
	$(MAKE) -s -C $(CAPACITY_LIB) check

register: all
	$(BIN)/billing-run --period 2026-07

demo: all
	@test -f meridian.db || $(BIN)/mediation --data data --db meridian.db > /dev/null
	@echo "inventory-api on :8081, invoice-api on :8082, dashboard pages on :8083"
	@echo "  capacity check: http://localhost:8083/capacity.html  (API: http://localhost:8081/capacity?requested=350)"
	@$(BIN)/inventory-api & \
	 $(BIN)/invoice-api & \
	 python3 -m http.server 8083 --directory dashboard & \
	 wait

clean:
	rm -f $(BIN)/* */*.o */*/*.o meridian.db

.PHONY: all run check register demo clean
