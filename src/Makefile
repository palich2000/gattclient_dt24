#INA219 = yes
MCP = yes
#ZIGBEEGATE = yes

CC ?= gcc

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
mqtt.o
#dzip.o \


DST=gattclient

INCLUDES = $(shell pkg-config --cflags glib-2.0)
EXTRA_LIBS = $(shell pkg-config --libs glib-2.0) -lpthread -lm -lcrypt -lrt -lmosquitto #-lzip

all: $(DST)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

$(DST): $(OBJGROUP)
	$(CC) -o $(DST) $(OBJGROUP) $(EXTRA_LIBS) -lm

DEPS = $(SRCS:%.c=%.d)


-include $(DEPS)

clean:
	rm -f *.o *.d $(DST) core

install: $(DST)
	install -D -o root -g root ./$(DST) /usr/local/bin

format:
	astyle -A2 -p -xg -k2 -W2 *.c *.h
check:
	cppcheck --enable=all --inconclusive --std=c11 --suppress=unusedFunction .

#######################
G_EX = $(shell git describe --tag > /dev/null ; if [ $$? -eq 0 ]; then echo "OK"; else echo "FAIL" ; fi)
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
