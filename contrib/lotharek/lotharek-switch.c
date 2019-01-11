/* lotharek-switch: configure 1050-2-PC/SIO2PC dual interface
   for 1050-2-PC or SIO2PC mode.

   Copyright (C) 2018-2019 Matthias Reichl <hias@horus.com>

   Based on bitbang_cusb example from libftdi

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
#include <unistd.h>
#include <stdlib.h>
#include <ftdi.h>
#include <libusb.h>

int main(int argc, char** argv)
{
	struct ftdi_context ftdic;
	int f;
	unsigned char bitmask;

	const char* serial = NULL;
	const char* arg;
	const char* mode_desc;

	switch (argc) {
	case 2:
		arg = argv[1];
		break;
	case 3:
		serial = argv[1];
		arg = argv[2];
		break;
	default:
		goto usage;
	}

	switch (arg[0]) {
	case '1':
		// 1050-2-PC mode
		bitmask = 0x80; // set CBUS3 LO
		mode_desc = "1050-2-PC";
		break;
	case 's':
	case 'S':
	case '0':
		bitmask = 0x88; // set CBUS3 HI
		mode_desc = "SIO2PC";
		break;
	default:
		goto usage;
	}

	if (ftdi_init(&ftdic) < 0) {
		fprintf(stderr, "ftdi_init failed\n");
		return 1;
	}

	f = ftdi_usb_open_desc(&ftdic, 0x0403, 0x6001, "10502PC/SIO2PC-USB", serial);

	if (f < 0 && f != -5) {
		fprintf(stderr, "unable to open 10502PC/SIO2PC-USB device: %d (%s)\n",
			f, ftdi_get_error_string(&ftdic));
		return 1;
	}

	f = libusb_set_auto_detach_kernel_driver(ftdic.usb_dev, 1);
	if (f) {
		printf("warning: failed to enable auto-detach mode: %d\n", f);
	}

	f = ftdi_set_bitmode(&ftdic, bitmask, BITMODE_CBUS);
	if (f < 0) {
		fprintf(stderr, "cannot configure interface for %s mode: error %d (%s)\n",
			mode_desc,
			f, ftdi_get_error_string(&ftdic));
	} else {
		printf("successfully switched interface to %s mode\n", mode_desc);
		ftdi_disable_bitbang(&ftdic);
	}

	ftdi_usb_close(&ftdic);
	ftdi_deinit(&ftdic);
	return 0;

usage:
	printf("usage: lotharek-switch [serialnr] 1050|SIO\n");
	return 1;
}
