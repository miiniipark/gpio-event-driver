SUMMARY = "GPIO event reader utility"
DESCRIPTION = "Command-line utility for reading GPIO button events from the /dev/gpio_event character device."
LICENSE = "CLOSED"

COMPATIBLE_MACHINE = "^beaglebone-yocto$"

SRC_URI = "file://read_event.c"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CPPFLAGS} ${CFLAGS} ${S}/read_event.c -o read_event ${LDFLAGS}
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 read_event ${D}${bindir}/read_event
}
