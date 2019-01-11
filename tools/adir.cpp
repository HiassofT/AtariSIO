/*
   adir.cpp - list directory of ATR image

   Copyright (C) 2002-2019 Matthias Reichl <hias@horus.com>

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

#include "AtrMemoryImage.h"
#include "SIOTracer.h"
#include "FileTracer.h"
#include "Dos2xUtils.h"
#include "Version.h"

static void print_single_directory(RCPtr<Dos2xUtils> utils, unsigned int level, unsigned int dirsec)
{
	RCPtr<Dos2xUtils::Dos2Dir> dir = utils->GetDos2Directory(false, dirsec);
	if (dir.IsNull()) {
		printf("error reading directory\n");
		return;
	}
	unsigned int i,j;
	for (i=0;i<dir->GetNumberOfFiles();i++) {
		for (j=0;j<level;j++) {
			printf("    ");
		}

		printf("%s\n", dir->GetFile(i));
		if ((dir->GetFileStatus(i) & 0xdf) == 0x10) {
			print_single_directory(utils, level+1, dir->GetFileStartingSector(i));
		}
	}
}

static void print_directory_tree(const RCPtr<AtrImage>& image)
{
	if (image.IsNull()) {
		printf("error - disk image is NULL\n");
		return;
	}
	RCPtr<Dos2xUtils> utils = new Dos2xUtils(image);
	RCPtr<Dos2xUtils::Dos2Dir> dir = utils->GetDos2Directory();
	if (dir.IsNull()) {
		printf("error reading directory\n");
		return;
	}

	print_single_directory(utils, 0, 361);

	printf("%d free sectors\n", dir->GetFreeSectors());
}


static void print_directory(const RCPtr<AtrImage>& image, unsigned int columns)
{
	if (image.IsNull()) {
		printf("error - disk image is NULL\n");
		return;
	}
	RCPtr<Dos2xUtils> utils = new Dos2xUtils(image);
	RCPtr<Dos2xUtils::Dos2Dir> dir = utils->GetDos2Directory();
	if (dir.IsNull()) {
		printf("error reading directory\n");
		return;
	}

	if (columns < 1) {
		columns = 1;
	}
	unsigned int i;
	for (i=0;i<dir->GetNumberOfFiles();i++) {
		printf("%s", dir->GetFile(i));
		if ( i % columns == (columns - 1)) {
			printf("\n");
		}
	}
	if (i % columns != 0) {
		printf("\n");
	}
	printf("%d free sectors\n", dir->GetFreeSectors());
}

static void print_raw_directory(const RCPtr<AtrImage>& image)
{
	if (image.IsNull()) {
		printf("error - disk image is NULL\n");
		return;
	}
	RCPtr<Dos2xUtils> utils = new Dos2xUtils(image);
	utils->DumpRawDirectory();
}

#ifdef ALL_IN_ONE
int adir_main(int argc, char**argv)
#else
int main(int argc, char**argv)
#endif
{
	int columns=4;
	char* filename;
	RCPtr<AtrMemoryImage> image;
	int idx=1;
	bool dumpraw = false;
	bool dumptree = false;

	SIOTracer* sioTracer = SIOTracer::GetInstance();
	{
		RCPtr<FileTracer> tracer(new FileTracer(stderr));
		RCPtr<FileTracer> tracer_stdout(new FileTracer(stdout));
		sioTracer->AddTracer(tracer);
		sioTracer->AddTracer(tracer_stdout);
		sioTracer->SetTraceGroup(SIOTracer::eTraceInfo, true, tracer_stdout);
		sioTracer->SetTraceGroup(SIOTracer::eTraceWarning, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceError, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceDebug, true, tracer);
	}


	if (argc < 2) {
		goto usage;
	}

	for (idx = 1; idx < argc && argv[idx][0] == '-'; idx++) {
		switch (argv[idx][1]) {
		case 'r':
			dumpraw = true;
			break;
		case 't':
			dumptree = true;
			break;
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			columns = atoi(argv[idx]+1);
			if (columns < 1 || columns > 64) {
				printf("illegal number of columns\n");
				goto usage;
			}
			break;
		default:
			printf("unknown option %s\n", argv[idx]);
			goto usage;
		}
	}

	image = new AtrMemoryImage;
	while (idx < argc) {
		filename = argv[idx];
		if (!image->ReadImageFromFile(filename)) {
			printf("reading image '%s' failed\n",filename);
		} else {
			printf("\ndirectory of image '%s'\n\n",filename);
			if (dumpraw) {
				print_raw_directory(image);
			} else {
				if (dumptree) {
					print_directory_tree(image);
				} else {
					print_directory(image, columns);
				}
			}
		}
		idx++;
	}

	SIOTracer::GetInstance()->RemoveAllTracers();
	return 0;
usage:
	printf("adir %s\n", VERSION_STRING);
	printf("(c) 2003-2019 Matthias Reichl <hias@horus.com>\n");
	printf("usage: adir [-<columns>] [-r] [-t] filename...\n");
	SIOTracer::GetInstance()->RemoveAllTracers();
	return 1;
}
