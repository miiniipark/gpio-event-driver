SUMMARY = "GPIO event demo image for BeagleBone Black"
DESCRIPTION = "Minimal image for the GPIO event platform driver project on BeagleBone Black."
LICENSE = "CLOSED"

inherit core-image

IMAGE_INSTALL:append = " \
    kernel-module-gpio-event-bbb-driver \
"

EXTRA_IMAGEDEPENDS:append:beaglebone-yocto = " \
    gpio-event-bbb-overlay \
"

IMAGE_BOOT_FILES:append:beaglebone-yocto = " \
    devicetree/gpio-event-bbb-overlay.dtbo;overlays/gpio-event-bbb-overlay.dtbo \
"
