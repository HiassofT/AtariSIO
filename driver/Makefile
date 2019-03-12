obj-m := atarisio.o

inst-m := $(wildcard *.ko)
ifeq ($(inst-m),)
inst-m := $(obj-m)
endif

ifeq ($(KERNELRELEASE),)

# first call to Makefile

ifneq ($(USE_KBUILD),1)

# use old build procedure

default: atarisio.o

MODFLAGS = -Wall -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -O2 -g -DMODULE -D__KERNEL__ -I$(KDIR)/include

atarisio.o: atarisio.c atarisio.h $(KDIR)/include/linux/version.h
	$(KERNEL_CC) $(MODFLAGS) -c atarisio.c

allclean: clean

clean:
	rm -f atarisio.o

dep:
	rm -f .depend
	for i in *.c ; do \
		$(KERNEL_CC) -I$(KDIR)/include -MM $$i >> .depend ; \
	done

else

# use kernel build system

PWD=$(shell pwd)

default:
	make -C $(KDIR) M=$(PWD) modules

allclean: clean

clean:
	make -C $(KDIR) M=$(PWD) clean

dep: # do nothing

endif

install: $(inst-m) atarisio.h
	if test ! -d $(MDIR) ; then \
		mkdir -p $(MDIR) ; \
	fi
	install -o root -g root -m 644 $(inst-m) $(MDIR)
	depmod -a
	if test ! -d $(INST_DIR) ; then \
		mkdir -p $(INST_DIR)/include/ ; \
	fi
	install -o root -g root -m 644 atarisio.h $(INST_DIR)/include/atarisio.h

uninstall:
	rm -f $(MDIR)/$(inst-m)
	depmod -a
	rm -f $(INST_DIR)/include/atarisio.h

endif

-include .depend

