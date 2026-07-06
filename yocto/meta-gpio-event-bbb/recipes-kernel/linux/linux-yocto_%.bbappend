FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append:beaglebone-yocto = " file://am335x-boneblack-gpio-event.dts"

do_configure:append:beaglebone-yocto() {
    install -d ${S}/arch/arm/boot/dts/ti/omap
    install -m 0644 ${WORKDIR}/am335x-boneblack-gpio-event.dts \
        ${S}/arch/arm/boot/dts/ti/omap/
}

KERNEL_DEVICETREE:append:beaglebone-yocto = " ti/omap/am335x-boneblack-gpio-event.dtb"
