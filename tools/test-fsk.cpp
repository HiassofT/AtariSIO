/*
   measure-system-latency: measure serial transmission timing parameters

   Copyright (C) 2006 Matthias Reichl <hias@horus.com>

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

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>

#include "SIOWrapper.h"
#include "SIOTracer.h"
#include "FileTracer.h"
#include "Error.h"
#include "MiscUtils.h"

using namespace MiscUtils;

#include "Version.h"

static RCPtr<SIOWrapper> SIO;
static SIOTracer* sioTracer = 0;

static void my_sig_handler(int sig)
{
	switch (sig) {
	case SIGALRM:
	case SIGINT:
	case SIGTERM:
	case SIGHUP:
		if (sioTracer) {
			sioTracer->RemoveAllTracers();
		}
		exit(0);
	}
}

static void send_fsk_data()
{
	unsigned int num_bits = 1000;
	uint16_t* delays = (uint16_t*) calloc(num_bits, sizeof(uint16_t));

	unsigned int i;
	for (i=0; i<num_bits; i++) {
		delays[i] = 10;
	}

	int ret = SIO->SendFskData(delays, num_bits);
	if (ret) {
		printf("SendFskData returned %d\n", ret);
	} else {
		printf("SendFskData OK\n");
	}

}

static void print_fsk(uint8_t byte)
{
	std::list<uint16_t> fsk;
	MiscUtils::ByteToFsk(byte, fsk, 1);

	std::list< uint16_t >::const_iterator iter;
	std::list< uint16_t >::const_iterator listend = fsk.end();
	printf("fsk data for %02x:\n", byte);
	for (iter=fsk.begin(); iter != listend; iter++) {
		printf("%2d\n", *iter);
	}
}

int main(int argc, char** argv)
{
	sioTracer = SIOTracer::GetInstance();
	RCPtr<FileTracer> tracer(new FileTracer(stderr));
	sioTracer->AddTracer(tracer);

	sioTracer->SetTraceGroup(SIOTracer::eTraceInfo, true, tracer);
	sioTracer->SetTraceGroup(SIOTracer::eTraceDebug, true, tracer);
	sioTracer->SetTraceGroup(SIOTracer::eTraceVerboseCommands, true, tracer);

	const char* siodev = 0;

	print_fsk(0);
	print_fsk(255);
	print_fsk(0x55);
	print_fsk(0xaa);
	print_fsk('A');
	return 0;

	if (argc >= 2) {
		siodev = argv[1];
	}

	try {
		SIO = SIOWrapper::CreateSIOWrapper(siodev);
	}
	catch (ErrorObject& err) {
		std::cerr << err.AsString() << std::endl;
		return 1;
	}
       	if (MiscUtils:: set_realtime_scheduling(0)) {
#ifdef ATARISIO_DEBUG
		ALOG("this program will automatically terminate in 5 minutes!");
		signal(SIGALRM, my_sig_handler);
		alarm(300);
#endif
	}

	if (SIO->Set1050CableType(SIOWrapper::e1050_2_PC)) {
		printf("cannot set cable type to 1050-2-pc\n");
		return 1;
	}

	send_fsk_data();

	sioTracer->RemoveAllTracers();
	return 0;
}
