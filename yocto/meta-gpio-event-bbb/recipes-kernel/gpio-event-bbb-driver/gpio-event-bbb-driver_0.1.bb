SUMMARY = "GPIO event driver for BeagleBone Black"
DESCRIPTION = "A Linux kernel module for BeagleBone Black that reports GPIO button events through a character device interface."
LICENSE = "CLOSED"

inherit module

SRC_URI = "file://Makefile \
           file://gpio-event-bbb-driver.c \
          "

S = "${WORKDIR}"

RPROVIDES:${PN} += "kernel-module-gpio-event-bbb-driver"
