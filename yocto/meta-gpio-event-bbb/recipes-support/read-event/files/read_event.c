#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_DEVICE_PATH "/dev/gpio_event"

static volatile sig_atomic_t stop;

static void handle_signal(int signo)
{
	(void)signo;
	stop = 1;
}

static void print_usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s [-d device] [-n count] [-t timeout_ms]\n"
		"\n"
		"Options:\n"
		"  -d device      device path, default: %s\n"
		"  -n count       number of events to read, default: 0(infinite)\n"
		"  -t timeout_ms  poll timeout in ms, default: -1(infinite)\n"
		"\n"
		"Examples:\n"
		"  %s\n"
		"  %s -n 10\n"
		"  %s -d /dev/gpio_event -t 5000\n",
		prog, DEFAULT_DEVICE_PATH, prog, prog, prog);
}

static void print_timestamp(void)
{
	struct timespec ts;
	struct tm tm;
	char buf[32];

	if (clock_gettime(CLOCK_REALTIME, &ts) < 0)
		return;

	localtime_r(&ts.tv_sec, &tm);
	strftime(buf, sizeof(buf), "%F %T", &tm);

	printf("[%s.%03ld] ", buf, ts.tv_nsec / 1000000);
}

int main(int argc, char *argv[])
{
	const char *device_path = DEFAULT_DEVICE_PATH;
	long max_events = 0;
	int timeout_ms = -1;
	int fd;
	int opt;
	long event_count = 0;

	while ((opt = getopt(argc, argv, "d:n:t:h")) != -1) {
		switch (opt) {
		case 'd':
			device_path = optarg;
			break;
		case 'n':
			max_events = strtol(optarg, NULL, 0);
			if (max_events < 0) {
				fprintf(stderr, "invalid count: %s\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 't':
			timeout_ms = (int)strtol(optarg, NULL, 0);
			if (timeout_ms < -1) {
				fprintf(stderr, "invalid timeout: %s\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'h':
		default:
			print_usage(argv[0]);
			return opt == 'h' ? EXIT_SUCCESS : EXIT_FAILURE;
		}
	}

	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);

	fd = open(device_path, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		fprintf(stderr, "failed to open %s: %s\n",
			device_path, strerror(errno));
		return EXIT_FAILURE;
	}

	printf("Reading button events from %s\n", device_path);
	printf("Press Ctrl+C to stop.\n\n");

	while (!stop && (max_events == 0 || event_count < max_events)) {
		struct pollfd pfd = {
			.fd = fd,
			.events = POLLIN,
		};
		char value;
		ssize_t n;
		int ret;

		ret = poll(&pfd, 1, timeout_ms);
		if (ret < 0) {
			if (errno == EINTR)
				continue;

			fprintf(stderr, "poll failed: %s\n", strerror(errno));
			close(fd);
			return EXIT_FAILURE;
		}

		if (ret == 0) {
			print_timestamp();
			printf("timeout: no event\n");
			continue;
		}

		if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
			fprintf(stderr, "poll error: revents=0x%x\n", pfd.revents);
			close(fd);
			return EXIT_FAILURE;
		}

		if (!(pfd.revents & POLLIN))
			continue;

		n = read(fd, &value, 1);
		if (n < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;

			fprintf(stderr, "read failed: %s\n", strerror(errno));
			close(fd);
			return EXIT_FAILURE;
		}

		if (n == 0)
			continue;

		event_count++;

		print_timestamp();

		if (value == '1') {
			printf("event=%ld button_value=1 state=PRESSED\n",
				event_count);
		} else if (value == '0') {
			printf("event=%ld button_value=0 state=RELEASED\n",
				event_count);
		} else {
			printf("event=%ld unknown_value=0x%02x\n",
				event_count, (unsigned char)value);
		}
	}

	close(fd);
	printf("\nStopped. total_events=%ld\n", event_count);

	return EXIT_SUCCESS;
}
