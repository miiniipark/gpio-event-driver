SUMMARY = "Extlinux configuration for BeagleBone Black GPIO event image"
DESCRIPTION = "Static extlinux.conf for booting BeagleBone Black with the GPIO event custom DTB."
LICENSE = "CLOSED"

inherit deploy

SRC_URI += "file://extlinux.conf"

do_deploy() {
    install -m 0644 -D ${WORKDIR}/extlinux.conf ${DEPLOYDIR}/extlinux.conf
}

addtask deploy after do_unpack
