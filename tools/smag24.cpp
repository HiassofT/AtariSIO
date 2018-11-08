/*
   ataridd - dump sector range from Atari disk to file

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
#include "AtpImage.h"
#include "AtrMemoryImage.h"

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

int main(int argc)
{
	if (argc > 1) {
        SIOTracer* sioTracer = SIOTracer::GetInstance();
	{
		RCPtr<FileTracer> tracer(new FileTracer(stderr));
		sioTracer->AddTracer(tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceInfo, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceDebug, true, tracer);
	}

	int ret;

	try {
		SIO = new SIOWrapper;
	}
	catch (ErrorObject& err) {
		std::cerr << err.AsString() << std::endl;
		exit (1);
	}
	set_realtime_scheduling(0);
	//ret= SIO->Set1050CableType(SIOWrapper::eApeProsystem);
	ret= SIO->Set1050CableType(SIOWrapper::e1050_2_PC);

	if (ret) {
		printf("cannot set cable type\n");
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

	unsigned int s;
	int c1, c2;

	for (s=1;s<=17;s++) {
		int my_s = 702+s;
		params.aux1 = my_s & 0xff;
		params.aux2 = my_s >> 8;
		ret = SIO->DirectSIO(params);
		if (ret != 0) {
			print_error(ret);
			return 1;
		}
		params.aux1 = 720 & 0xff;
		params.aux2 = 720 >> 8;
		ret = SIO->DirectSIO(params);
		if (ret != 0) {
			print_error(ret);
			return 1;
		}
		c1 = buf[5];
		ret = SIO->DirectSIO(params);
		if (ret != 0) {
			print_error(ret);
			return 1;
		}
		c2 = buf[5];

		printf("%2d (%c): %02x , %02x\n", s, s+64, c1, c2);
	}
	} else {
		RCPtr<AtpImage> img = new AtpImage;
		RCPtr<AtpSector> sector;
		img->SetDensity(Atari1050Model::eDensityFM);
		char skew[] = "ACEGIKMROQDFHJLNPRRBR";
		//char skew[] = "ACEGIKMRFOQDHJLNPRRBR";
		unsigned int slen1 = 9100;
		unsigned int slen2 = 9900;
		uint8_t buf0[128];
		uint8_t buf255[128];
		uint8_t tmpbuf[128];
		memset(buf0, 0, 128);
		memset(buf255, 255, 128);

		RCPtr<AtrMemoryImage> atrImg = new AtrMemoryImage();
		if (!atrImg->ReadImageFromFile("smag24b.atr")) {
			printf("cannot read ATR image\n");
			return 1;
		}
		int s;
		int sidx, track;
		unsigned int spos;

		for (s=1;s<=702;s++) {
			sidx = ((s-1) % 18) + 1;
			track = (s-1) / 18;
			if (!atrImg->ReadSector(s, tmpbuf, 128)) {
				printf("error reading ATR sector!\n");
				return 1;
			}
			spos = Atari1050Model::CalculatePositionOfSDSector(track, sidx);
			sector = new AtpSector(sidx, 128, tmpbuf, spos, 
					Atari1050Model::eSDSectorTimeLength);
			img->AddSector(track, sector);
		}

		unsigned int startpos = Atari1050Model::CalculatePositionOfSDSector(39, 1);

		for (s=0;s<21;s++) {
			int sidx = skew[s] - 64;
			spos = (startpos + s*slen2) % Atari1050Model::eDiskRotationTime;
			uint8_t* buf = buf0;
			if (sidx == 18 && s >= 18) {
				buf = buf255;
			}
			sector = new AtpSector(sidx, 128, buf, spos, slen1);
			img->AddSector(39, sector);
		}
		img->WriteImageToFile("smag24.atp");
	}

	return 0;
}
