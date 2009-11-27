/*
   txtiming - test UART transmission timing

   Copyright (C) 2008 Matthias Reichl <hias@horus.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdio.h>
#include <string.h>
#include <sys/io.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <linux/serial.h>
#include <linux/serial_reg.h>

#include <sched.h>
#include <unistd.h>

unsigned int serial_base = 0;
unsigned int baud_base = 0;

uint8_t serial_in(unsigned int offset)
{
	return inb_p(serial_base + offset);
}

void serial_out(unsigned int offset, uint8_t val)
{
	outb(val, serial_base + offset);
}

uint8_t old_lsr = 0;
uint8_t old_msr = 0;

void print_timestamp()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	printf("%ld.%06ld: ", tv.tv_sec, tv.tv_usec);
}

void print_bin(uint8_t c)
{
	int i;
	char buf[9];
	buf[8] = 0;
	for (i=0; i<8; i++) {
		if (c & (1<<(7-i))) {
			buf[i] = '1';
		} else {
			buf[i] = '0';
		}
	}
	printf("%s", buf);
}

void print_lsr()
{
	uint8_t lsr = serial_in(UART_LSR);
	uint8_t msr = serial_in(UART_MSR);
	if ( (lsr != old_lsr) || (msr != old_msr) ) {
		print_timestamp();
		printf("LSR: ");
		print_bin(lsr);
		printf(" MSR: ");
		print_bin(msr);
		printf("\n");
		old_lsr = lsr;
		old_msr = msr;
	}
}

void single_test(unsigned int chars)
{
	struct timeval tv;
	struct timeval tv2;
	unsigned int i;

	gettimeofday(&tv, NULL);
	tv.tv_sec += 2; // 2 seconds of sleep

	do {
		print_lsr();
		gettimeofday(&tv2, NULL);
	} while (tv2.tv_sec <= tv.tv_sec);

	for (i=0;i<chars;i++) {
		serial_out(UART_TX, 0x42);
	}
	print_timestamp();
	printf("wrote %d chars\n", chars);
	tv.tv_sec += 2; // 2 seconds of sleep

	do {
		print_lsr();
		gettimeofday(&tv2, NULL);
	} while (tv2.tv_sec <= tv.tv_sec);

}

#define MAXRX 1000

inline uint64_t tv_to_ts(struct timeval& tv)
{
	return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

static void set_realtime_scheduling()
{
	struct sched_param sp;
	pid_t myPid;
	myPid = getpid();

	memset(&sp, 0, sizeof(struct sched_param));
	sp.sched_priority = sched_get_priority_max(SCHED_RR);

	if (sched_setscheduler(myPid, SCHED_RR, &sp) == 0) {
		printf("activated realtime scheduling\n");
	} else {
		printf("setting realtime scheduling failed!\n");
	}
}

void rxonly_test(unsigned int seconds)
{
	struct timeval tv;
	struct timeval tv2;
	unsigned int i = 0;
	unsigned int j;

	uint8_t* msr_list = new uint8_t[MAXRX];
	unsigned long* ts_list = new unsigned long[MAXRX];

	uint64_t start_time, current_time;

	uint8_t msr, old_msr;

	sleep(1);
	set_realtime_scheduling();

	gettimeofday(&tv, NULL);
	start_time = tv_to_ts(tv);

	old_msr = serial_in(UART_MSR);

	do {
		msr = serial_in(UART_MSR);

		gettimeofday(&tv2, NULL);
		current_time = tv_to_ts(tv2);

		if ((i == 0) || msr != old_msr) {
			old_msr = msr;
			msr_list[i] = msr;
			ts_list[i] = current_time - start_time;
			i++;
		}
	} while (i < MAXRX && current_time < start_time + seconds * 1000000);

	for (j=0; j<i; j++) {
		printf("%06ld: LSR: 00000000 MSR: ", ts_list[j]);
		print_bin(msr_list[j]);
		printf("\n");
	}
}

void do_test(bool rxonly)
{
	/*
	printf("iobase: %04x\n", serial_base);

	unsigned int i;
	for (i=0;i<8;i++) {
		printf("%d: %02x\n", i, serial_in(i));
	}

	serial_out(UART_SCR, 0xaa);
	printf("%02x\n", serial_in(UART_SCR));
	serial_out(UART_SCR, 0x55);
	printf("%02x\n", serial_in(UART_SCR));

	return;
	*/

	uint8_t c;
	unsigned int divisor = baud_base / 19200;
	
	serial_out(UART_LCR, UART_LCR_WLEN8 | UART_LCR_DLAB);
	serial_out(UART_DLL, divisor & 0xff);
	serial_out(UART_DLM, divisor >> 8);
	serial_out(UART_LCR, UART_LCR_WLEN8);

	serial_out(UART_IER, 0);
	serial_out(UART_MCR, 0);

	serial_out(UART_FCR, 0);
	serial_out(UART_FCR, UART_FCR_ENABLE_FIFO | UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);
	serial_out(UART_FCR, UART_FCR_ENABLE_FIFO | UART_FCR_TRIGGER_1);

	c = serial_in(UART_LSR);
	c = serial_in(UART_IIR);
	c = serial_in(UART_RX);
	c = serial_in(UART_MSR);

	old_lsr = serial_in(UART_LSR) ^ 0xff;

	if (rxonly) {
		rxonly_test(5);
	} else {
		single_test(1);
		single_test(2);
		single_test(3);
		single_test(14);
	}
}

int main(int argc, char** argv)
{
	int i;
	bool rxonly = false;
	
	if (argc < 2) {
		printf("usage: txtiming port\n");
		return 1;
	}

	if (argc > 2) {
		rxonly = true;
	}

	struct serial_struct orig_serial;
	struct serial_struct new_serial;

	FILE* f=fopen(argv[1],"rw");
	if (!f) {
		printf("cannot open port %s\n", argv[1]);
		return 1;
	}
	if (ioctl(fileno(f), TIOCGSERIAL, &orig_serial)) {
		printf("cannot get serial port info\n");
		fclose(f);
		return 1;
	}

	ioctl(fileno(f), TIOCGSERIAL, &new_serial);
	new_serial.type = PORT_UNKNOWN;
	if (ioctl(fileno(f), TIOCSSERIAL, &new_serial)) {
		printf("cannot disable serial port\n");
		fclose(f);
		goto fail_reenable;
	}

	fclose(f);

	serial_base = orig_serial.port;
	baud_base = orig_serial.baud_base;

	i = iopl(3);
	printf("iopl = %d\n", i);

	if (i) {
		goto fail_reenable;
	}

	do_test(rxonly);

fail_reenable:
	f=fopen(argv[1],"rw");
	if (ioctl(fileno(f), TIOCSSERIAL, &orig_serial)) {
		printf("error re-enabling serial port\n");
		fclose(f);
		return 1;
	}
	fclose(f);
	return 0;
	
}
