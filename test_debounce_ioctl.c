#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define GPIO_EVENT_IOC_MAGIC 'g'
#define GPIO_EVENT_IOC_SET_DEBOUNCE_MS _IOW(GPIO_EVENT_IOC_MAGIC, 1, unsigned int)
#define GPIO_EVENT_IOC_GET_DEBOUNCE_MS _IOR(GPIO_EVENT_IOC_MAGIC, 2, unsigned int)

int main(int argc, char *argv[])
{
    int fd;
    unsigned int value;

    fd = open("/dev/gpio_event", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (ioctl(fd, GPIO_EVENT_IOC_GET_DEBOUNCE_MS, &value) < 0) {
        perror("ioctl GET");
        close(fd);
        return 1;
    }

    printf("current debounce_ms = %u\n", value);

    if (argc >= 2) {
        value = atoi(argv[1]);

        if (ioctl(fd, GPIO_EVENT_IOC_SET_DEBOUNCE_MS, &value) < 0) {
            perror("ioctl SET");
            close(fd);
            return 1;
        }

        printf("set debounce_ms = %u\n", value);

        if (ioctl(fd, GPIO_EVENT_IOC_GET_DEBOUNCE_MS, &value) < 0) {
            perror("ioctl GET after SET");
            close(fd);
            return 1;
        }

        printf("new debounce_ms = %u\n", value);
    }

    close(fd);
    return 0;
}