FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://am335x-boneblack-gpio-event.dts"

do_configure:append() {
    install -m 0644 ${WORKDIR}/am335x-boneblack-gpio-event.dts \
        ${S}/arch/arm/boot/dts/ti/omap/
}

KERNEL_DEVICETREE:append = " ti/omap/am335x-boneblack-gpio-event.dtb"
