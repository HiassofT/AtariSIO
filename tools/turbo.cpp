/*
   turbo.cpp - turbo 1050 test

   Copyright (C) 2004 Matthias Reichl <hias@horus.com>

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
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include <sched.h>
#include <string.h>

#include "SIOWrapper.h"
#include "SIOTracer.h"
#include "FileTracer.h"
#include "Error.h"

#include "Version.h"

static RCPtr<SIOWrapper> SIO;

static unsigned int drive_no = 1;

static void set_realtime_scheduling(int priority)
{
	struct sched_param sp;
	pid_t myPid;

	memset(&sp, 0, sizeof(struct sched_param));
	sp.sched_priority = sched_get_priority_max(SCHED_RR) - priority;
	myPid = getpid();

	if (sched_setscheduler(myPid, SCHED_RR, &sp) == 0) {
		printf("activated realtime scheduling\n");
	} else {
		printf("cannot set realtime scheduling! please run as root!\n");
	}

	seteuid(getuid());
	setegid(getgid());
}

static void print_error(int error)
{
	if (error > ATARISIO_ERRORBASE) {
		printf("[atari error %d]\n",error - ATARISIO_ERRORBASE);
	} else {
		printf("[system error %d]\n",error);
	}
}

static void hexdump(void* buf, int len)
{
	int i;
	for (i=0;i<len;i++) {
		printf("%02x ", ((uint8_t*)buf)[i]);
		if (i % 16 == 15) {
			printf("\n");
		}
	}
	if (len % 16) {
		printf("\n");
	}
}

static void print_result(const SIO_parameters& params, int ret)
{
	printf("[%02x %02x %02x %02x] : %d\n",
		params.device_id, params.command,
		params.aux1, params.aux2, ret);
}

int main(int argc, char** argv)
{
	bool prosys = true;
	int ret;

        SIOTracer* sioTracer = SIOTracer::GetInstance();
	{
		RCPtr<FileTracer> tracer(new FileTracer(stderr));
		sioTracer->AddTracer(tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceInfo, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceDebug, true, tracer);
	}

	try {
		SIO = SIOWrapper::CreateSIOWrapper();
	}
	catch (ErrorObject& err) {
		std::cerr << err.AsString() << std::endl;
		exit (1);
	}
	set_realtime_scheduling(0);
	if (prosys) {
		ret = SIO->Set1050CableType(SIOWrapper::eApeProsystem);
	} else {
		ret = SIO->Set1050CableType(SIOWrapper::e1050_2_PC);
	}

	if (ret) {
		printf("cannot set %s mode\n", prosys ? "prosystem" : "1050-2-PC");
		return 1;
	}

	SIO_parameters params;
	uint8_t buf[256];

	params.device_id = 0x31;
	params.timeout = 7;
	params.data_buffer = buf;
	params.data_length = 128;
	params.aux1 = 1;
	params.aux2 = 0;

	// set parameters
	buf[0] = 62; // align time
	buf[1] = 0; // special params ?
	buf[2] = 0; // ??
	buf[3] = 0; // no align, otherwise 62?
	buf[4] = 0xfe; // align reference sector
	buf[5] = 0; // align reference track
	buf[6] = 210; // track time - default for 288RPM
	//buf[7] = 0; // don't change RPM
	buf[7] = 0x60; // default - 288RPM

	params.command = 0x59;
	params.direction = 1;

	ret = SIO->DirectSIO(params);
	print_result(params, ret);

	return 0;
}
