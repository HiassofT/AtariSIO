/*
   dir2atr - create ATR image from directory

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
	bool autorun;
};

// standard boot entry, without MyPicoDos autorun
#define BOOT_ENTRY(name) \
{ #name, Dos2xUtils::eBoot##name, false },

// MyPicoDos boot entry supporting autorun mode
#define BOOT_ENTRY_A(name) \
{ #name, Dos2xUtils::eBoot##name, true },

struct BootEntry BootTypeTable[] = {
BOOT_ENTRY(Dos20)
BOOT_ENTRY(Dos25)
BOOT_ENTRY(MyDos4533)
BOOT_ENTRY(MyDos4534)
BOOT_ENTRY(MyDos455Beta4)
BOOT_ENTRY(TurboDos21)
BOOT_ENTRY(TurboDos21HS)
BOOT_ENTRY(XDos243F)
BOOT_ENTRY(XDos243N)

BOOT_ENTRY(MyPicoDos403)
BOOT_ENTRY(MyPicoDos403HS)

BOOT_ENTRY_A(MyPicoDos404)
BOOT_ENTRY_A(MyPicoDos404N)
BOOT_ENTRY_A(MyPicoDos404R)
BOOT_ENTRY_A(MyPicoDos404RN)
BOOT_ENTRY_A(MyPicoDos404B)

BOOT_ENTRY_A(MyPicoDos405)
BOOT_ENTRY_A(MyPicoDos405A)
BOOT_ENTRY_A(MyPicoDos405N)
BOOT_ENTRY_A(MyPicoDos405R)
BOOT_ENTRY_A(MyPicoDos405RA)
BOOT_ENTRY_A(MyPicoDos405RN)
BOOT_ENTRY_A(MyPicoDos405B)
BOOT_ENTRY_A(MyPicoDos405S0)
BOOT_ENTRY_A(MyPicoDos405S1)

{ "PicoBoot405", Dos2xUtils::ePicoBoot405, false },

BOOT_ENTRY_A(MyPicoDos406)
BOOT_ENTRY_A(MyPicoDos406A)
BOOT_ENTRY_A(MyPicoDos406N)
BOOT_ENTRY_A(MyPicoDos406R)
BOOT_ENTRY_A(MyPicoDos406RA)
BOOT_ENTRY_A(MyPicoDos406RN)
BOOT_ENTRY_A(MyPicoDos406B)
BOOT_ENTRY_A(MyPicoDos406S0)
BOOT_ENTRY_A(MyPicoDos406S1)


{ "PicoBoot406", Dos2xUtils::ePicoBoot406, false },

{ NULL, Dos2xUtils::eBootDefault, false }
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
	bool autorun = false;
	Dos2xUtils::EPicoNameType piconametype = Dos2xUtils::eNoPicoName;
	bool autoSectors = false;
	int sectors;
	char* directory;
	char* atrfilename;
	ESectorLength seclen;
	unsigned int idx;

	struct stat statbuf;

	Dos2xUtils::EBootType bootType = Dos2xUtils::eBootDefault;
	bool userDefBoot = false;
	unsigned char userDefBootData[384];

	int ret = 0;

	int c;
	while ( (c = getopt(argc, argv, "admpPb:B:")) != -1) {
		switch(c) {
		case 'a': autorun = true; break;
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
				printf("error: unknown boot sector type \"%s\"\n", optarg);
				goto usage;
			}
			break;
		case 'B':
			FILE* bootfile = fopen(optarg, "rb");
			if (bootfile == NULL) {
				printf("error: cannot open boot sector file \"%s\"\n", optarg);
				goto usage;
			}
			if (fread(userDefBootData, 1, 384, bootfile) != 384) {
				printf("error reading boot sector data from \"%s\"\n", optarg);
				fclose(bootfile);
				goto usage;
			} else {
				printf("loaded boot sector data from \"%s\"\n", optarg);
			}
			fclose(bootfile);
			userDefBoot = true;
			break;
		}
	}

	if (autorun) {
		if (BootTypeTable[idx].autorun == false) {
			printf("autorun not supported for %s boot sectors\n",
				BootTypeTable[idx].name);
			goto usage;
		} else {
			printf("enabled MyPicoDos autorun mode\n");
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
			printf("error: illegal number of sectors - must be 720..65535\n");
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
		if (sectors > 65535) {
			printf("error: calculated disk size %d is larger than maximum of 65535\n",
				sectors);
			return 1;
		}
		printf("calculated disk size is %d sectors\n", sectors);
	}

	if ( !((sectors == 720) || (sectors == 1040 && !dd) )) {
		if (!mydos) {
			printf("non-standard disk size - using MyDOS format\n");
			mydos = true;
		}
	}

	image = new AtrMemoryImage;
	if (!image->CreateImage(seclen, sectors)) {
		printf("error: cannot create atr image\n");
		return 1;
	}

	observer = new VirtualImageObserver(image);
	dos2xutils = new Dos2xUtils(image, directory, observer.GetRealPointer());
	observer->SetRootDirectoryObserver(dos2xutils);

	if (mydos) {
		if (!dos2xutils->SetDosFormat(Dos2xUtils::eMyDos)) {
			printf("error: cannot set MyDos format\n");
			return 1;
		}
	} else {
		if (!dos2xutils->SetDosFormat(Dos2xUtils::eDos2x)) {
			printf("error: cannot set DOS 2.x format\n");
			return 1;
		}
	}
	if (!dos2xutils->InitVTOC()) {
		printf("error: creating directory failed\n");
		return 1;
	}

	if (!dos2xutils->AddBootFile(bootType)) {
		printf("error: adding boot file failed\n");
		return 1;
	}
	if (!dos2xutils->AddFiles(piconametype)) {
		printf("error: failed to add all files\n");
		ret = 1;
	}

	if (userDefBoot) {
		for (int i = 0; i < 3; i++) {
			image->WriteSector(i+1, userDefBootData+i*128, 128);
		}
	} else {
		if (!dos2xutils->WriteBootSectors(bootType, autorun)) {
			printf("error: writing boot sectors failed\n");
			ret = 1;
		}
	}

	if (image->WriteImageToFile(atrfilename)) {
		printf("created image \"%s\"\n", atrfilename);
	} else {
		printf("error writing image to \"%s\"\n", atrfilename);
		ret = 1;
	}

	dos2xutils = 0;
	observer = 0;

	SIOTracer::GetInstance()->RemoveAllTracers();
	return ret;
usage:
	printf("dir2atr %s\n", VERSION_STRING);
	printf("(c) 2004-2019 Matthias Reichl <hias@horus.com>\n");
	printf("usage: dir2atr [-admpP] [-b <DOS>] [-B file] [sectors] atrfile directory\n");
	printf("  -d        create double density image (default: single density)\n");
	printf("  -m        create MyDOS image (default: DOS 2.x)\n");
	printf("  -p        create PICONAME.TXT (long filename description)\n");
	printf("  -P        create PICONAME.TXT (with file extensions stripped)\n");
	printf("  -a        enable MyPicoDos autorun mode\n");
	printf("  -b <DOS>  create bootable disk for specified DOS\n");
	printf("            Supported DOS are: Dos20, Dos25, \n");
	printf("            MyDos4533, MyDos4534, MyDos455Beta4\n");
	printf("            TurboDos21, TurboDos21HS, XDos243F, XDos243N,\n");
	printf("            MyPicoDos403, MyPicoDos403HS,\n");
	printf("            MyPicoDos404, MyPicoDos404N, MyPicoDos404R, MyPicoDos404RN,\n");
	printf("            MyPicoDos404B,\n");
	printf("            MyPicoDos405, MyPicoDos405A, MyPicoDos405N,\n");
	printf("            MyPicoDos405R, MyPicoDos405RA, MyPicoDos405RN,\n");
	printf("            MyPicoDos405B, MyPicoDos405S0, MyPicoDos405S1, PicoBoot405\n");
	printf("            MyPicoDos406, MyPicoDos406A, MyPicoDos406N\n");
	printf("            MyPicoDos406R, MyPicoDos406RA, MyPicoDos406RN\n");
	printf("            MyPicoDos406B, MyPicoDos406S0, MyPicoDos406S1, PicoBoot406\n");
	printf("  -B <FILE> load boot sector data from <FILE>\n");

	SIOTracer::GetInstance()->RemoveAllTracers();
	return 1;
}
