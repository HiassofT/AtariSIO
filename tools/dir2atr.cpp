/*
   dir2atr - create ATR image from directory

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "AtrMemoryImage.h"
#include "SIOTracer.h"
#include "FileTracer.h"
#include "Dos2xUtils.h"
#include "VirtualImageObserver.h"
#include "Version.h"

#ifdef ALL_IN_ONE
int dir2atr_main(int argc, char**argv)
#else
int main(int argc, char**argv)
#endif
{
	RCPtr<AtrMemoryImage> image;
	RCPtr<Dos2xUtils> dos2xutils;
	RCPtr<VirtualImageObserver> observer;

	SIOTracer* sioTracer = SIOTracer::GetInstance();
	{
		RCPtr<FileTracer> tracer(new FileTracer(stderr));
		sioTracer->AddTracer(tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceInfo, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceDebug, true, tracer);
	}


	bool dd = false;
	bool mydos = false;
	bool piconame = false;
	bool autoSectors = false;
	int sectors;
	char* directory;
	char* atrfilename;
	ESectorLength seclen;

	struct stat statbuf;

	Dos2xUtils::EBootType bootType = Dos2xUtils::eBootDefault;

	char c;
	while ( (c = getopt(argc, argv, "dmpb:")) != -1) {
		switch(c) {
		case 'd': dd = true; printf("using double density sectors\n"); break;
		case 'm': mydos = true; printf("using mydos format\n"); break;
		case 'p': piconame = true; printf("creating PICONAME.TXT\n"); break;
		case 'b':
			if (strcasecmp(optarg,"dos20") == 0) {
				bootType = Dos2xUtils::eBootDos20;
			} else if (strcasecmp(optarg,"dos25") == 0) {
				bootType = Dos2xUtils::eBootDos25;
			} else if (strcasecmp(optarg,"mydos4533") == 0) {
				bootType = Dos2xUtils::eBootMyDos4533;
			} else if (strcasecmp(optarg,"mydos4534") == 0) {
				bootType = Dos2xUtils::eBootMyDos4534;
			} else if (strcasecmp(optarg,"turbodos21") == 0) {
				bootType = Dos2xUtils::eBootTurboDos21;
			} else if (strcasecmp(optarg,"turbodos21hs") == 0) {
				bootType = Dos2xUtils::eBootTurboDos21HS;
			} else if (strcasecmp(optarg,"mypicodos403") == 0) {
				bootType = Dos2xUtils::eBootMyPicoDos403;
			} else if (strcasecmp(optarg,"mypicodos403hs") == 0) {
				bootType = Dos2xUtils::eBootMyPicoDos403HS;

			} else if (strcasecmp(optarg,"mypicodos404") == 0) {
				bootType = Dos2xUtils::eBootMyPicoDos404;
			} else if (strcasecmp(optarg,"mypicodos404n") == 0) {
				bootType = Dos2xUtils::eBootMyPicoDos404N;
			} else if (strcasecmp(optarg,"mypicodos404r") == 0) {
				bootType = Dos2xUtils::eBootMyPicoDos404R;
			} else if (strcasecmp(optarg,"mypicodos404rn") == 0) {
				bootType = Dos2xUtils::eBootMyPicoDos404RN;
			} else if (strcasecmp(optarg,"mypicodos404b") == 0) {
				bootType = Dos2xUtils::eBootMyPicoDos404B;

			} else if (strcasecmp(optarg,"mypicodos405") == 0) {
				bootType = Dos2xUtils::eBootMyPicoDos405;
			} else if (strcasecmp(optarg,"mypicodos405a") == 0) {
				bootType = Dos2xUtils::eBootMyPicoDos405A;
			} else if (strcasecmp(optarg,"mypicodos405n") == 0) {
				bootType = Dos2xUtils::eBootMyPicoDos405N;
			} else if (strcasecmp(optarg,"mypicodos405r") == 0) {
				bootType = Dos2xUtils::eBootMyPicoDos405R;
			} else if (strcasecmp(optarg,"mypicodos405ra") == 0) {
				bootType = Dos2xUtils::eBootMyPicoDos405RA;
			} else if (strcasecmp(optarg,"mypicodos405rn") == 0) {
				bootType = Dos2xUtils::eBootMyPicoDos405RN;
			} else if (strcasecmp(optarg,"mypicodos405b") == 0) {
				bootType = Dos2xUtils::eBootMyPicoDos405B;
			} else if (strcasecmp(optarg,"mypicodos405s0") == 0) {
				bootType = Dos2xUtils::eBootMyPicoDos405S0;
			} else if (strcasecmp(optarg,"mypicodos405s1") == 0) {
				bootType = Dos2xUtils::eBootMyPicoDos405S1;
			} else if (strcasecmp(optarg,"picoboot405") == 0) {
				bootType = Dos2xUtils::ePicoBoot405;
			} else {
				printf("unknown boot sector type \"%s\"\n", optarg);
			}
		}
	}

	if (dd) {
		seclen = e256BytesPerSector;
	} else {
		seclen = e128BytesPerSector;
	}

	if (argc == optind+2) {
		autoSectors = true;
	} else if (argc != optind+3) {
		goto usage;
	}

	if (!autoSectors) {
		sectors = atoi(argv[optind++]);
		if (sectors < 720 || sectors > 65535) {
			printf("illegal sector number - must be 720..65535\n");
			return 1;
		}
	}

	atrfilename=argv[optind++];
	directory=argv[optind++];

	// check if directory exists
	if (stat(directory, &statbuf)) {
		printf("error: cannot stat directory \"%s\"\n", directory);
		return 1;
	}
	if (!S_ISDIR(statbuf.st_mode)) {
		printf("error: \"%s\" is not a directory\n", directory);
		return 1;
	}

	if (autoSectors) {
		if (!mydos) {
			printf("number of sectors not specified - using MyDOS format\n");
			mydos = true;
		}

		sectors = Dos2xUtils::EstimateDiskSize(directory, seclen, piconame, bootType);
		printf("calculated disk size is %d sectors\n", sectors);
	}

	if ( !((sectors == 720) || (sectors == 1040 && !dd) )) {
		if (!mydos) {
			printf("non-standard disk size - using MyDOS format\n");
			mydos = true;
		}
	}

	image = new AtrMemoryImage;
	if (dd) {
		if (!image->CreateImage(seclen, sectors)) {
			printf("cannot create atr image\n");
			return 1;
		}
	} else {
		if (!image->CreateImage(seclen, sectors)) {
			printf("cannot create atr image\n");
			return 1;
		}
	}

	observer = new VirtualImageObserver(image);
	dos2xutils = new Dos2xUtils(image, directory, observer.GetRealPointer());
	observer->SetRootDirectoryObserver(dos2xutils);

	if (mydos) {
		if (!dos2xutils->SetDosFormat(Dos2xUtils::eMyDos)) {
			printf("cannot set MyDos format\n");
			return 1;
		}
	} else {
		if (!dos2xutils->SetDosFormat(Dos2xUtils::eDos2x)) {
			printf("cannot set DOS 2.x format\n");
			return 1;
		}
	}
	if (!dos2xutils->InitVTOC()) {
		printf("creating directory failed\n");
		return 1;
	}

	if (!dos2xutils->AddBootFile(bootType)) {
		printf("adding boot file failed\n");
		return 1;
	}
	dos2xutils->AddFiles(piconame);

	if (!dos2xutils->WriteBootSectors(bootType)) {
		printf("writing boot sectors failed\n");
	}

	if (!image->WriteImageToFile(atrfilename)) {
		printf("error writing image to \"%s\"\n", atrfilename);
	}

	dos2xutils = 0;
	observer = 0;

	SIOTracer::GetInstance()->RemoveAllTracers();
	return 0;
usage:
	printf("dir2atr " VERSION_STRING " (c) 2004-2009 by Matthias Reichl\n");
	printf("usage: dir2atr [-d] [-m] [-p] [-b <DOS>] [sectors] atrfile directory\n");
	printf("  -d        create double density image (default: single density)\n");
	printf("  -m        create MyDOS image (default: DOS 2.x)\n");
	printf("  -p        create PICONAME.TXT (long filename description)\n");
	printf("  -b <DOS>  create bootable disk for specified DOS\n");
	printf("            Supported DOS are: Dos20, Dos25, MyDos4533, MyDos4534\n");
	printf("            TurboDos21, TurboDos21HS, MyPicoDos403, MyPicoDos403HS,\n");
	printf("            MyPicoDos404, MyPicoDos404N, MyPicoDos404R, MyPicoDos404RN,\n");
	printf("            MyPicoDos404B, MyPicoDos405, MyPicoDos405A, MyPicoDos405N,\n");
	printf("            MyPicoDos405R, MyPicoDos405RA, MyPicoDos405RN, MyPicoDos405B,\n");
	printf("            MyPicoDos405S0, MyPicoDos405S1, PicoBoot405\n");

	SIOTracer::GetInstance()->RemoveAllTracers();
	return 1;
}
