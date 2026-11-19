#INA219 = yes
MCP = yes
#ZIGBEEGATE = yes

CC = gcc

CFLAGS += -g -std=c11 -MD -MP  -Wall -Werror -Wfatal-errors -Wextra  -g -I.

OBJGROUP = att.o \
bluetooth.o \
btgattclient.o \
crypto.o \
gatt-client.o \
gatt-db.o \
gatt-helpers.o \
hci.o \
io-mainloop.o \
mainloop.o \
queue.o \
timeout-glib.o \
util.o \
uuid.o \
dexec.o \
dfork.o \
dlog.o \
dmem.o \
dnonblock.o \
dpid.o \
dsignal.o \
mqtt.o \
app_config.o \
mqtt_transport.o
#dzip.o \


DST=gattclient

INCLUDES = $(shell pkg-config --cflags gio-2.0)
EXTRA_LIBS = $(shell pkg-config --libs gio-2.0) -lpthread -lm -lcrypt -lrt -lmosquitto

RUUVI_OBJ = ruuvi_main.o ruuvi.o ruuvi_state.o app_config.o mqtt_transport.o dlog.o
TEST_BINS = ../tests/test_ruuvi ../tests/test_config ../tests/test_state ../tests/test_transport ../tests/test_dt24

all: $(DST) ruuvi-mqtt

COMBINED_OBJ = $(filter-out btgattclient.o,$(OBJGROUP)) btgattclient_combined.o ruuvi_raw.o hci_adv.o ruuvi.o ruuvi_state.o

gattclient-ruuvi: $(COMBINED_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(EXTRA_LIBS)

btgattclient_combined.o: btgattclient.c
	$(CC) $(CFLAGS) $(INCLUDES) -DGATT_RUUVI -c $< -o $@

../tests/test_hci_adv: ../tests/test_hci_adv.c hci_adv.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(EXTRA_LIBS)

../tests/test_ruuvi_raw: ../tests/test_ruuvi_raw.c ruuvi_raw.o hci_adv.o ruuvi_state.o ruuvi.o app_config.o mainloop.o hci.o bluetooth.o dlog.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(EXTRA_LIBS) -Wl,--wrap=hci_open_dev,--wrap=setsockopt,--wrap=hci_le_set_scan_parameters,--wrap=hci_le_set_scan_enable,--wrap=g_get_monotonic_time

../tests/test_combined_mqtt: ../tests/test_combined_mqtt.c mqtt.o app_config.o dlog.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(EXTRA_LIBS)

../tests/test_dual_mqtt: ../tests/test_dual_mqtt.c mqtt.o mqtt_transport.o app_config.o dlog.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(EXTRA_LIBS)

combined-integration-test: $(DST) combined-test ../tests/test_dual_mqtt
	python3 ../tests/test_observer.py
	python3 ../tests/test_install_combined.py
	python3 ../tests/test_update_quiet.py
	python3 ../tests/test_combined_config.py ./gattclient-ruuvi
	python3 ../tests/test_dual_mqtt.py ../tests/test_dual_mqtt

../tests/test_health_main: ../tests/test_health_main.c btgattclient.c $(filter-out btgattclient_combined.o,$(COMBINED_OBJ))
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(filter-out btgattclient_combined.o,$(COMBINED_OBJ)) $(EXTRA_LIBS)

combined-test: gattclient-ruuvi ../tests/test_health_main ../tests/test_hci_adv ../tests/test_ruuvi_raw ../tests/test_combined_mqtt
	../tests/test_hci_adv
	../tests/test_ruuvi_raw
	python3 ../tests/test_health_logs.py ../tests/test_ruuvi_raw ../tests/test_health_main
	../tests/test_combined_mqtt
	./gattclient-ruuvi -D -d 70:BF:48:9E:1B:42 --ruuvi DE:4D:F8:3F:DA:91 --check-config


ruuvi-mqtt: $(RUUVI_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(EXTRA_LIBS)

../tests/test_ruuvi: ../tests/test_ruuvi.c ruuvi.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(EXTRA_LIBS)
../tests/test_config: ../tests/test_config.c app_config.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(EXTRA_LIBS)
../tests/test_state: ../tests/test_state.c ruuvi_state.o ruuvi.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(EXTRA_LIBS)
../tests/test_transport: ../tests/test_transport.c mqtt_transport.o app_config.o dlog.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(EXTRA_LIBS)

../tests/test_dt24: ../tests/test_dt24.c mqtt.o mqtt_transport.o app_config.o dlog.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(EXTRA_LIBS)

test: $(TEST_BINS)
	../tests/test_ruuvi
	../tests/test_config ../tests/config-valid.ini
	../tests/test_state

integration-test: test all
	python3 ../tests/test_mqtt.py ../tests/test_transport
	python3 ../tests/test_mqtt.py ../tests/test_transport --tls
	python3 ../tests/test_mqtt.py ../tests/test_dt24
	python3 ../tests/test_cli.py ./gattclient
	python3 ../tests/test_cli.py ./ruuvi-mqtt
	dbus-run-session -- /usr/bin/python3 ../tests/test_bluez.py "$(CURDIR)/ruuvi-mqtt"
	dbus-run-session -- /usr/bin/python3 ../tests/test_recovery.py "$(CURDIR)/ruuvi-mqtt"

.PHONY: all test integration-test clean

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

$(DST): $(OBJGROUP)
	$(CC) -o $(DST) $(OBJGROUP) $(EXTRA_LIBS) -lm

DEPS = $(sort $(OBJGROUP:.o=.d) $(RUUVI_OBJ:.o=.d) $(COMBINED_OBJ:.o=.d))


-include $(DEPS)

clean:
	rm -f *.o *.d $(DST) ruuvi-mqtt gattclient-ruuvi $(TEST_BINS) ../tests/test_hci_adv ../tests/test_ruuvi_raw ../tests/test_combined_mqtt ../tests/test_dual_mqtt ../tests/test_health_main ../tests/*.d core

install: $(DST)
	install -D -o root -g root ./$(DST) /usr/local/bin

format:
	astyle -A2 -p -xg -k2 -W2 *.c *.h
check:
	cppcheck --enable=all --inconclusive --std=c11 --suppress=unusedFunction .

#######################
G_EX = $(shell git describe --tag > /dev/null 2>&1 ; if [ $$? -eq 0 ]; then echo "OK"; else echo "FAIL" ; fi)
GVER = $(shell git describe --abbrev=7 --long)
#######################

version.c: FORCE
	@echo "==============================================="
	@echo "git present:" $(G_EX) " ver:" $(GVER)
	@echo "==============================================="
ifeq "$(G_EX)" "OK"
	git describe --tag | awk 'BEGIN { FS="-" } {print "#include \"version.h\""} {print "const char * git_version = \"" $$1"."$$2"\";"} END {}' > version.c
	git rev-parse --abbrev-ref HEAD | awk '{print "const char * git_branch = \""$$0"\";"} {}' >> version.c
endif

FORCE:
