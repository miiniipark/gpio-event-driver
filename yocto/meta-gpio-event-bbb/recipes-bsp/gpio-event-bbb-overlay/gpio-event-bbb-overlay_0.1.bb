SUMMARY = "GPIO event Device Tree overlay for BeagleBone Black"
DESCRIPTION = "Device Tree overlay for GPIO event driver using GPIO48 as button and GPIO60 as LED on BeagleBone Black."
LICENSE = "CLOSED"

inherit devicetree

COMPATIBLE_MACHINE = "^beaglebone-yocto$"
SRC_URI:beaglebone-yocto = "file://gpio-event-bbb-overlay.dts"

S = "${WORKDIR}"
