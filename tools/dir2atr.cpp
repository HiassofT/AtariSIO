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

struct BootEntry {
	const char* name;
	Dos2xUtils::EBootType bootType;
};

#define BOOT_ENTRY(name) \
{ #name, Dos2xUtils::eBoot##name },

struct BootEntry BootTypeTable[] = {
BOOT_ENTRY(Dos20)
BOOT_ENTRY(Dos25)
BOOT_ENTRY(MyDos4533)
BOOT_ENTRY(MyDos4534)
BOOT_ENTRY(TurboDos21)
BOOT_ENTRY(TurboDos21HS)
BOOT_ENTRY(XDos243F)
BOOT_ENTRY(XDos243N)

BOOT_ENTRY(MyPicoDos403)
BOOT_ENTRY(MyPicoDos403HS)

BOOT_ENTRY(MyPicoDos404)
BOOT_ENTRY(MyPicoDos404N)
BOOT_ENTRY(MyPicoDos404R)
BOOT_ENTRY(MyPicoDos404RN)
BOOT_ENTRY(MyPicoDos404B)

BOOT_ENTRY(MyPicoDos405)
BOOT_ENTRY(MyPicoDos405A)
BOOT_ENTRY(MyPicoDos405N)
BOOT_ENTRY(MyPicoDos405R)
BOOT_ENTRY(MyPicoDos405RA)
BOOT_ENTRY(MyPicoDos405RN)
BOOT_ENTRY(MyPicoDos405B)
BOOT_ENTRY(MyPicoDos405S0)
BOOT_ENTRY(MyPicoDos405S1)

{ "PicoBoot405", Dos2xUtils::ePicoBoot405 },
{ NULL, Dos2xUtils::eBootDefault }
};

#undef BOOT_ENTRY

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
		sioTracer->SetTraceGroup(SIOTracer::eTraceWarning, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceError, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceDebug, true, tracer);
	}


	bool dd = false;
	bool mydos = false;
	Dos2xUtils::EPicoNameType piconametype = Dos2xUtils::eNoPicoName;
	bool autoSectors = false;
	int sectors;
	char* directory;
	char* atrfilename;
	ESectorLength seclen;
	unsigned int idx;

	struct stat statbuf;

	Dos2xUtils::EBootType bootType = Dos2xUtils::eBootDefault;

	char c;
	while ( (c = getopt(argc, argv, "dmpPb:")) != -1) {
		switch(c) {
		case 'd': dd = true; printf("using double density sectors\n"); break;
		case 'm': mydos = true; printf("using mydos format\n"); break;
		case 'p': piconametype = Dos2xUtils::ePicoName; printf("creating PICONAME.TXT\n"); break;
		case 'P': piconametype = Dos2xUtils::ePicoNameWithoutExtension; printf("creating PICONAME.TXT (without file extensions)\n"); break;
		case 'b':
			idx = 0;
			while (BootTypeTable[idx].name && strcasecmp(optarg, BootTypeTable[idx].name)) {
				idx++;
			}
			if (BootTypeTable[idx].name) {
				bootType = BootTypeTable[idx].bootType;
			} else {
				printf("unknown boot sector type \"%s\"\n", optarg);
				goto usage;
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

		sectors = Dos2xUtils::EstimateDiskSize(directory, seclen, piconametype, bootType);
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
	dos2xutils->AddFiles(piconametype);

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
	printf("dir2atr " VERSION_STRING " (c) 2004-2010 by Matthias Reichl\n");
	printf("usage: dir2atr [-d] [-m] [-p] [-b <DOS>] [sectors] atrfile directory\n");
	printf("  -d        create double density image (default: single density)\n");
	printf("  -m        create MyDOS image (default: DOS 2.x)\n");
	printf("  -p        create PICONAME.TXT (long filename description)\n");
	printf("  -P        create PICONAME.TXT (with file extensions stripped)\n");
	printf("  -b <DOS>  create bootable disk for specified DOS\n");
	printf("            Supported DOS are: Dos20, Dos25, MyDos4533, MyDos4534,\n");
	printf("            TurboDos21, TurboDos21HS, XDos243F, XDos243N,\n");
	printf("            MyPicoDos403, MyPicoDos403HS,\n");
	printf("            MyPicoDos404, MyPicoDos404N, MyPicoDos404R, MyPicoDos404RN,\n");
	printf("            MyPicoDos404B,\n");
	printf("            MyPicoDos405, MyPicoDos405A, MyPicoDos405N, MyPicoDos405R\n");
	printf("            MyPicoDos405RA, MyPicoDos405RN, MyPicoDos405B,\n");
	printf("            MyPicoDos405S0, MyPicoDos405S1, PicoBoot405\n");

	SIOTracer::GetInstance()->RemoveAllTracers();
	return 1;
}
