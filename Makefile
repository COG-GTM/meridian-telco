CXX = g++
CXXFLAGS = -O2 -Wall
LDFLAGS = -lsqlite3
TLS_LDFLAGS = -lssl -lcrypto
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
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(TLS_LDFLAGS)

$(BIN)/billing-run: $(BILLING_OBJS) billing/run_billing.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN)/invoice-api: $(BILLING_OBJS) billing/invoice-api/main.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(TLS_LDFLAGS)

$(BIN)/billing-test: $(BILLING_OBJS) billing/rules_test.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN)/httpd-test: inventory-api/httpd_test.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(TLS_LDFLAGS)

$(BIN)/ipam: network/ipam.o | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

inventory-api/main.o billing/invoice-api/main.o inventory-api/httpd_test.o: inventory-api/httpd.h

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

run: all
	$(BIN)/mediation --data data --db meridian.db

check: $(BIN)/billing-test $(BIN)/httpd-test
	$(BIN)/billing-test
	$(BIN)/httpd-test

register: all
	$(BIN)/billing-run --period 2026-07

demo: all
	@echo "invoice-api on 127.0.0.1:8082, invoice register page on 127.0.0.1:8083"
	@MERIDIAN_API_TOKEN=$${MERIDIAN_API_TOKEN:-$$(openssl rand -hex 32)}; export MERIDIAN_API_TOKEN; \
	 echo "api token for the dashboard prompt: $$MERIDIAN_API_TOKEN"; \
	 $(BIN)/invoice-api & \
	 python3 -m http.server 8083 --bind 127.0.0.1 --directory dashboard & \
	 wait

clean:
	rm -f $(BIN)/* */*.o */*/*.o meridian.db

.PHONY: all run check register demo clean
