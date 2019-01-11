/*
   ataridd - dump sector range from Atari disk to file

   Copyright (C) 2004-2019 Matthias Reichl <hias@horus.com>

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

int main(int argc, char** argv)
{
        SIOTracer* sioTracer = SIOTracer::GetInstance();
	{
		RCPtr<FileTracer> tracer(new FileTracer(stderr));
		sioTracer->AddTracer(tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceInfo, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceWarning, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceError, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceDebug, true, tracer);
	}

	int c;
	int ret;

	SIOWrapper::E1050CableType cable_type = SIOWrapper::e1050_2_PC;
	const char* cable_desc = "1050-2-PC";

	unsigned int start_sector = 1, end_sector=720;

	printf("ataridd %s\n(c) 2002-2019 by Matthias Reichl <hias@horus.com>\n\n",VERSION_STRING);
	while ((c = getopt(argc, argv, "pls:e:")) != -1) {
		switch (c) {
		case 'p':
			cable_type = SIOWrapper::eApeProsystem;
			cable_desc = "APE prosystem";
			break;
		case 'l':
			cable_type = SIOWrapper::eLotharekSwitchable;
			cable_desc = "Lotharek 1050-2-PC USB";
			break;
		case 's':
			start_sector = (unsigned int) atoi(optarg);
			if (start_sector < 1 || start_sector > 65535) {
				goto usage;
			}
			break;
		case 'e':
			end_sector = (unsigned int) atoi(optarg);
			if (end_sector < 1 || end_sector > 65535) {
				goto usage;
			}
			break;
		default:
			printf("forgot to catch option %d\n",c);
			break;
		}
	}
	if (optind + 1 != argc) {
		goto usage;
	}

	try {
		SIO = SIOWrapper::CreateSIOWrapper();
	}
	catch (ErrorObject& err) {
		std::cerr << err.AsString() << std::endl;
		exit (1);
	}
	set_realtime_scheduling(0);

	ret = SIO->Set1050CableType(cable_type);

	if (ret) {
		printf("cannot set %s mode\n", cable_desc);
		return 1;
	}

	SIO_parameters params;
	uint8_t buf[128];

	params.device_id = 0x31;
	params.command = 'R';
	params.direction = 0;
	params.timeout = 7;
	params.data_length = 128;
	params.data_buffer = buf;

	FILE* out;
	unsigned int s, len;

	out = fopen(argv[optind], "w");
	if (!out) {
		printf("cannot open %s\n", argv[optind]);
		return 1;
	}

	for (s=start_sector;s<=end_sector;s++) {
		params.aux1 = s & 0xff;
		params.aux2 = s >> 8;
		ret = SIO->DirectSIO(params);
		if (ret != 0) {
			print_error(ret);
			fclose(out);
			return 1;
		}
		len = fwrite(buf, 1, 128, out);
		if (len != 128) {
			printf("cannot write to image file\n");
			fclose(out);
			return 1;
		}
		
	}
	fclose(out);
	printf("wrote %d sectors to %s\n",
		end_sector - start_sector + 1, argv[optind]);
	return 0;
usage:
	printf("usage: [-pl] [-s start_sector] [-e end_sector] imagefile\n\n");
	printf("  -p           use APE prosystem cable (default: 1050-2-PC cable)\n");
	printf("  -l           use Lotharek 1050-2-PC USB cable");
	return 1;
}
