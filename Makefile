KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

obj-m += gpio_event_driver.o

USER_TARGET := read_event
USER_SRC    := read_event.c

USER_CC      ?= gcc
USER_CFLAGS  ?= -Wall -Wextra -O2

.PHONY: all module user clean

all: module user

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user: $(USER_TARGET)

$(USER_TARGET): $(USER_SRC)
	$(USER_CC) $(USER_CFLAGS) -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) $(USER_TARGET)