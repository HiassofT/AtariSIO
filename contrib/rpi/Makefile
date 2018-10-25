OVERLAYS = uart-ctsrts.dtbo uart-ctsrts-p5.dtbo uart-rts.dtbo \
	miniuart-ctsrts.dtbo

DTC ?= dtc

%.dtbo: %.dts
	$(DTC) -@ -H epapr -O dtb -o $@ -Wno-unit_address_vs_reg $^

.phony: all
all: $(OVERLAYS$)

.phony: install
install:
	cp -f $(OVERLAYS) /boot/overlays

.phony: clean
clean:
	rm -f *.dtbo
