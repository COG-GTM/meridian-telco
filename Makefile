CXX = g++
RULES = third_party/unified-inventory-rules
CXXFLAGS = -O2 -Wall -I$(RULES)/cpp/include
LDFLAGS = -lsqlite3
BIN = bin

MEDIATION_OBJS = $(RULES)/cpp/src/capacity.o mediation/status.o mediation/circuit_counter.o mediation/store.o mediation/locations.o
BILLING_OBJS = billing/rating.o billing/discounts.o billing/accounts.o

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

$(BIN)/ipam: network/ipam.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

run: all
	$(BIN)/mediation --data data --db meridian.db

clean:
	rm -f $(BIN)/* */*.o */*/*.o $(RULES)/cpp/src/*.o meridian.db

.PHONY: all run clean
