EXECUTABLES = lotharek-switch

all: $(EXECUTABLES)

INST_DIR ?= /usr/local

CC = gcc
CXX = g++

CFLAGS = -g -W -Wall
LDFLAGS = -g

LIBFTDI_CFLAGS ?= $(shell ./getlibftdi.sh --cflags)
LIBFTDI_LDFLAGS ?= $(shell ./getlibftdi.sh --libs)

LIBUSB_CFLAGS ?= $(shell ./getlibusb.sh --cflags)
LIBUSB_LDFLAGS ?= $(shell ./getlibusb.sh --libs)

CFLAGS += $(LIBFTDI_CFLAGS)
CFLAGS += $(LIBUSB_CFLAGS)

lotharek-switch: lotharek-switch.o
	$(CC) $(LDFLAGS) -o $@ $< $(LIBFTDI_LDFLAGS) $(LIBUSB_LDFLAGS)

.phony: install

install: $(EXECUTABLES)
	install -o root -g root -m 0755 -d $(INST_DIR)/bin
	install -o root -g root -m 0755 $(EXECUTABLES) $(INST_DIR)/bin

.phony: clean
clean:
	rm -f *.o $(EXECUTABLES)
