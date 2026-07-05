SUMMARY = "GPIO event character device kernel module"
DESCRIPTION = "A Linux kernel module that reports GPIO button events through a character device interface."
LICENSE = "CLOSED"

inherit module

SRC_URI = "file://Makefile \
           file://gpio_event_driver.c \
          "

S = "${WORKDIR}"

RPROVIDES:${PN} += "kernel-module-gpio-event"
