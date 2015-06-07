# Allow overriding xeno-config on make command line
XENO_CONFIG=xeno-config

prefix := $(shell $(XENO_CONFIG) --prefix)

ifeq ($(prefix),)
$(error Please add <xenomai-install-path>/bin to your PATH variable)
endif

PWD:= $(shell pwd)
KDIR := /lib/modules/$(shell uname -r)/build
CC := $(shell $(XENO_CONFIG) --skin=native --cc)
STD_CFLAGS  := $(shell $(XENO_CONFIG) --skin=native --cflags) -I. -g
STD_LDFLAGS := $(shell $(XENO_CONFIG) --skin=native --ldflags) -g -lrtdm

EXTRA_CFLAGS += $(shell xeno-config --skin=native --cflags)
EXTRA_CFLAGS += $(CFLAGS)

obj-m += gpio_irq_rtdm.o


all: gpio-irq-test gpio_irq_rtdm.ko
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

gpio_irq_rtdm.ko: gpio-irq.h

gpio-irq-test: gpio-irq-test.c gpio-irq.h
	$(CC) -o $@ $< $(STD_CFLAGS) $(STD_LDFLAGS)

install:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules_install
	depmod -a

clean:
	rm -f *~ Module.markers Module.symvers modules.order
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean
	$(RM) -f *.o *~ gpio-irq-test


