SUMMARY = "GPIO event demo image for BeagleBone Black"
DESCRIPTION = "Minimal image for the GPIO event platform driver project on BeagleBone Black."
LICENSE = "CLOSED"

inherit core-image

IMAGE_INSTALL:append = " \
    kernel-module-gpio-event-bbb-driver \
"

do_image_wic[depends] += "gpio-event-bbb-extlinux:do_deploy"

IMAGE_BOOT_FILES:append = " \
    am335x-boneblack-gpio-event.dtb \
    extlinux.conf;extlinux/extlinux.conf \
"
