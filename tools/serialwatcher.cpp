/*
   serialwatcher.cpp - watch input of serial port

   Copyright (C) 2003, 2004 Matthias Reichl <hias@horus.com>

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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

#include "linux/serial.h"

#include "AtariDebug.h"
#include "SIOTracer.h"
#include "FileTracer.h"
#include "MiscUtils.h"
using namespace MiscUtils;

static int serial_fd;

static serial_struct orig_ss;

void my_signal_handler(int sig)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		if (ioctl(serial_fd, TIOCSSERIAL, &orig_ss)) {
			DPRINTF("set serial info failed");
		}
		close(serial_fd);
		serial_fd = -1;
		exit(0);
		break;
	}
}

bool OpenSerial(const char* device)
{
	serial_fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
	//serial_fd = open(device, O_RDWR | O_NOCTTY);
	if (serial_fd < 0) {
		printf("cannot open %s\n", device);
		return false;
	}
	
	struct termios tio;
	tcgetattr(serial_fd, &tio);
	cfmakeraw(&tio);

	/* set flow control to NONE */
	tio.c_cflag &= ~(CRTSCTS);
	tio.c_iflag &= ~(IXON | IXOFF | IXANY);

	/* set serial port communication parameters */
	tio.c_cflag |= CS8; /* 8 data bits */
	tio.c_cflag &= ~CSTOPB; /* 1 stop bit */
	tio.c_cflag &= ~PARENB; /* disable parity */
	tio.c_cflag &= ~CBAUD; /* clear baud rate bits */
	tio.c_cflag |= B19200; /* set bit rate */

	tio.c_cflag |= CLOCAL; /* modem control off */
	tio.c_cflag |= CREAD; /* enable reads from port */
	
	tio.c_lflag &= ~ICANON; /* turn off line by line mode */
	tio.c_lflag &= ~ECHO; /* no echo of TX characters */
	tio.c_lflag &= ~ISIG; /* no input ctrl char checking */
	tio.c_lflag &= ~IEXTEN; /* no extended input processing */
	
	tio.c_iflag &= ~ICRNL; /* do not map CR to NL on in */
	tio.c_iflag &= ~IGNCR; /* allow CR characters */
	tio.c_iflag &= ~INLCR; /* do not map NL to CR on in */
	tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK) ; /* report breaks as '0' */
	tio.c_iflag &= ~ISTRIP; /* don't strip 8th bit */
	tio.c_iflag &= ~INPCK; /* no input parity checking */
	
	tio.c_oflag &= ~OCRNL; /* do not map CR to NL on out */
	tio.c_oflag &= ~OPOST; /* no output ctrl char checking*/

	//tio.c_cc[VMIN] = 0; /* read can return with no chars */
	tio.c_cc[VMIN] = 0; /* read at least one character */
	tio.c_cc[VTIME] = 0; /* read may return immediately */

	if (tcsetattr(serial_fd, TCSANOW, &tio) != 0) {
		DPRINTF("tcsetattr failed");
		return false;
	}

	serial_struct ss;
	if (ioctl(serial_fd, TIOCGSERIAL, &ss)) {
		DPRINTF("get serial info failed");
		return false;
	}
	
	if (ioctl(serial_fd, TIOCGSERIAL, &orig_ss)) {
		DPRINTF("get serial info failed");
		return false;
	}
	
	// enable low latency
	// ss.flags |= ASYNC_LOW_LATENCY;

	// set 16450 type to disable fifo
	// ss.type = PORT_8250;

	if (ioctl(serial_fd, TIOCSSERIAL, &ss)) {
		DPRINTF("set serial info failed");
	}
	tcflush(serial_fd, TCIOFLUSH);	/* flush input and output buffer */

	//write(serial_fd,"12345", 5);

	return true;
}

static void print_timestamp(bool timestamp_printed)
{
	if (timestamp_printed) {
		printf("                           ");
	} else {
		static uint64_t start_time;
		static uint64_t last_timestamp;
		static bool first=true;

		uint64_t current_timestamp = GetCurrentTime();
		if (first) {
			start_time = current_timestamp;
			last_timestamp = current_timestamp;
			first = false;
		}
		printf("%10lu [ +%10lu ] ", 
				(unsigned long) (current_timestamp - start_time),
				(unsigned long) (current_timestamp - last_timestamp));
		last_timestamp = current_timestamp;
	}
	timestamp_printed = true;
}

static void print_linechange(int new_value)
{
	printf("line = [ ");
	if (new_value & TIOCM_CTS) {
		printf("CTS ");
	} else {
		printf("    ");
	}

	if (new_value & TIOCM_DSR) {
		printf("DSR ");
	} else {
		printf("    ");
	}

	if (new_value & TIOCM_RNG) {
		printf("RNG ");
	} else {
		printf("    ");
	}
	printf("]\n");
}

//const unsigned int maxsize = 16;
const unsigned int maxsize = 1;

uint8_t buf[maxsize];

int main(int argc, char** argv)
{
	if (argc != 2) {
		printf("usage: serialwatcher device\n");
		return 1;
	}

        SIOTracer* sioTracer = SIOTracer::GetInstance();
	{
		RCPtr<FileTracer> tracer(new FileTracer(stderr));
		sioTracer->AddTracer(tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceInfo, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceWarning, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceError, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceDebug, true, tracer);
	}

	if (!OpenSerial(argv[1])) {
		printf("cannot open serial device %s\n", argv[1]);
		sioTracer->RemoveAllTracers();
		return 1;
	}

	unsigned int old_msr=0, new_msr;

	signal(SIGINT, my_signal_handler);
	signal(SIGTERM, my_signal_handler);

	bool timestamp_printed;

	while (1) {
		ioctl(serial_fd, TIOCMGET, &new_msr);
		ssize_t count = read(serial_fd, buf, maxsize);
		timestamp_printed = false;
		if ((count>0) || (old_msr != new_msr) ) {
			if (old_msr != new_msr) {
				print_timestamp(timestamp_printed);
				print_linechange(new_msr);
				old_msr = new_msr;
			}
			if (count > 0) {
				print_timestamp(timestamp_printed);
				printf("got %d bytes: ", (int) count);
				for (ssize_t i=0; i<count; i++) {
					printf("%02x ", buf[i]);
				}
				printf("\n");
			}
		}
	}
	sioTracer->RemoveAllTracers();
	return 0;
}
