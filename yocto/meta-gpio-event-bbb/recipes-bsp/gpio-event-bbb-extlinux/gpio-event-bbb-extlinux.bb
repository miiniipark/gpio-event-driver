SUMMARY = "Extlinux configuration for BeagleBone Black GPIO event image"
DESCRIPTION = "Static extlinux.conf for booting the BeagleBone Black GPIO event image with a custom DTB."
LICENSE = "CLOSED"

COMPATIBLE_MACHINE = "^beaglebone-yocto$"

inherit deploy

SRC_URI = "file://extlinux.conf"

do_deploy() {
    install -m 0644 -D ${WORKDIR}/extlinux.conf ${DEPLOYDIR}/extlinux.conf
}

addtask deploy after do_unpack before do_build
