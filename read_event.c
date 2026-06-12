#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

struct gpio_event {
	uint32_t seq;
	int value;
};

int main(void)
{
	int fd;
	struct gpio_event event;

	fd = open("/dev/gpio_event", O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	while (1) {
		if (read(fd, &event, sizeof(event)) != sizeof(event)) {
			perror("read");
			break;
		}

		printf("seq=%u value=%d\n", event.seq, event.value);
	}

	close(fd);
	return 0;
}