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

int set_rts(int fd, int on)
{
	int rts = TIOCM_RTS;
	return ioctl(fd, on ? TIOCMBIS : TIOCMBIC, &rts);
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

#define MAXLEN 65536
static unsigned char buf[MAXLEN];

int write_bytes(int fd, int bytes, int call_tcdrain)
{
	unsigned long long ts_start, ts_end;
	int ret;

	ts_start = get_timestamp();
	set_rts(fd, 1);
	if (write(fd, &buf, bytes) != bytes) {
		printf("writing %d bytes failed\n", bytes);
		return 1;
	}
	if (call_tcdrain) {
		usleep(1000);
		ret = tcdrain(fd);
		if (ret) {
			printf("tcdrain returned %d\n", ret);
		}
	} else {
		usleep(1000);
	}
	set_rts(fd, 0);
	ts_end = get_timestamp();
	printf("writing %d bytes plus %s took %lluusec\n",
		bytes,
		call_tcdrain ? "tcdrain" : "1ms delay",
		ts_end - ts_start);

	return 0;
}


int main(int argc, char** argv)
{
	int fd;
	int bytes = 1;

	printf("argc = %d\n", argc);

	if (argc < 2) {
		printf("usage: tcdrain-test /dev/ttySx\n");
		return 1;
	}
	fd = init_serial_port(argv[1]);
	if (fd < 0) {
		return 1;
	}

	if (argc > 2) {
		bytes = atoi(argv[2]);
		if (bytes <= 0 || bytes > MAXLEN) {
			bytes = 1;
		}
	}

	memset(buf, 0x55, bytes);

	set_rts(fd, 0);

	usleep(10000);
	write_bytes(fd, 1, 0);
	usleep(10000);
	write_bytes(fd, 1, 0);
	usleep(10000);
	write_bytes(fd, bytes, 1);
	usleep(10000);
	write_bytes(fd, 1, 0);
	usleep(10000);

	close(fd);
	return 0;
}
