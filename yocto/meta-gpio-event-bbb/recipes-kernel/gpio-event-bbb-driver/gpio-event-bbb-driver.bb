SUMMARY = "GPIO event kernel module for BeagleBone Black"
DESCRIPTION = "Linux kernel module for BeagleBone Black that exposes GPIO button events through the /dev/gpio_event character device."
LICENSE = "CLOSED"

COMPATIBLE_MACHINE = "^beaglebone-yocto$"

inherit module

SRC_URI = " \
    file://Makefile \
    file://gpio-event-bbb-driver.c \
"

S = "${WORKDIR}"

RPROVIDES:${PN} += "kernel-module-gpio-event-bbb-driver"
