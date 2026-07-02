KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

obj-m += gpio_event_driver.o

USER_TARGET := read_event
USER_SRC    := read_event.c

USER_CC      ?= gcc
USER_CFLAGS  ?= -Wall -Wextra -O2

DTBO_TARGET := gpio-event.dtbo
DTBO_SRC    := gpio-event-overlay.dts

DTC ?= dtc

.PHONY: all module user dtbo clean

all: module user dtbo

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user: $(USER_TARGET)

$(USER_TARGET): $(USER_SRC)
	$(USER_CC) $(USER_CFLAGS) -o $@ $<

dtbo: $(DTBO_TARGET)

$(DTBO_TARGET): $(DTBO_SRC)
	$(DTC) -@ -I dts -O dtb -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) $(USER_TARGET) $(DTBO_TARGET)