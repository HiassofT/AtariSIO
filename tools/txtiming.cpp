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
#include <sys/io.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/serial.h>
#include <linux/serial_reg.h>

unsigned int serial_base = 0;
unsigned int baud_base = 0;

unsigned char serial_in(unsigned int offset)
{
	return inb_p(serial_base + offset);
}

void serial_out(unsigned int offset, unsigned char val)
{
	outb(val, serial_base + offset);
}

unsigned char old_lsr = 0;

void print_timestamp()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	printf("%ld.%06ld: ", tv.tv_sec, tv.tv_usec);
}

void print_lsr()
{
	unsigned char lsr = serial_in(UART_LSR);
	if (lsr != old_lsr) {
		print_timestamp();
		printf("%02x\n", lsr);
		old_lsr = lsr;
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

void do_test()
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

	unsigned char c;
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

	single_test(1);
	single_test(2);
	single_test(3);
	single_test(14);

}

int main(int argc, char** argv)
{
	int i,j;
	
	if (argc != 2) {
		printf("usage: txtiming port\n");
		return 1;
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

	do_test();

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
