#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

struct gpio_event {
    uint32_t seq;
    int value;
};

int main(void)
{
    int fd;
    struct pollfd pfd;
    struct gpio_event event;
    ssize_t n;

    fd = open("/dev/gpio_event", O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    pfd.fd = fd;
    pfd.events = POLLIN;

    printf("waiting for gpio event...\n");

    while (1) {
        int ret = poll(&pfd, 1, -1);

        if (ret < 0) {
            perror("poll");
            break;
        }

        if (pfd.revents & POLLIN) {
            n = read(fd, &event, sizeof(event));

            if (n < 0) {
                perror("read");
                break;
            }

            if (n == sizeof(event)) {
                printf("event: seq=%u value=%d\n",
                       event.seq, event.value);
            }
        }
    }

    close(fd);
    return 0;
}