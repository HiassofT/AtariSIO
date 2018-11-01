#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <linux/serial.h>

int set_low_latency(int fd)
{
	struct serial_struct ss;
	int ret;

	ret = ioctl(fd, TIOCGSERIAL, &ss);
	if (ret) {
		printf("TIOCGSERIAL failed: %d\n", ret);
		return ret;
	}
	ss.flags |= ASYNC_LOW_LATENCY;
	ret = ioctl(fd, TIOCSSERIAL, &ss);
	if (ret) {
		printf("enabling low latency failed: %d\n", ret);
	}
	return ret;
}

int init_serial_port(const char* port)
{
	int fd;
	int ret;
	struct termios tio;

	fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);

	if (fd <0) {
		printf("cannot open %s\n", port);
		return fd;
	}

	set_low_latency(fd);

	ret = ioctl(fd, TCGETS, &tio);

	if (ret) {
		printf("TCGETS failed");
		return ret;
	}

	cfmakeraw(&tio);
	tio.c_cc[VMIN] = 0;
	tio.c_cc[VTIME] = 0;
	//cfsetspeed(&tio, B115200);
	cfsetspeed(&tio, B19200);

	ret = ioctl(fd, TCSETS, &tio);

	if (ret) {
		printf("configuring serial port failed: %d\n", ret);
		return ret;
	}

	return fd;
}

unsigned long long get_timestamp()
{
	struct timeval tv;
	if (gettimeofday(&tv, NULL)) {
		printf("gettimeofday failed\n");
		return 0;
	}
	return (unsigned long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

int test_modem_status(int fd)
{
	unsigned long long ts_start, ts_end;
	int i;
	int flags;

	for (i = 0; i < 10; i++) {
		ts_start = get_timestamp();
		if (ioctl(fd, TIOCMGET, &flags)) {
			printf("TIOCMGET failed\n");
			break;
		}
		ts_end = get_timestamp();
		printf("TIOCMGET took %llu usec\n",
			ts_end - ts_start);
	}

	return 0;
}


int main(int argc, char** argv)
{
	int fd;

	if (argc < 2) {
		printf("usage: tiocmget-timing /dev/ttySx\n");
		return 1;
	}
	fd = init_serial_port(argv[1]);
	if (fd < 0) {
		return 1;
	}

	test_modem_status(fd);

	close(fd);
	return 0;
}
