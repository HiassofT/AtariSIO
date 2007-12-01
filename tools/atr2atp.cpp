/*
   atr2atp.cpp - convert ATR image into ATP image

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

#include "AtrMemoryImage.h"
#include "AtpImage.h"
#include "AtpUtils.h"
#include "SIOTracer.h"
#include "FileTracer.h"

#ifdef ALL_IN_ONE
int atr2atp_main(int argc, char** argv)
#else
int main(int argc, char** argv)
#endif
{
	if (argc != 3) {
		printf("usage: atr2atp in-file out-file\n");
		return 1;
	}

        SIOTracer* sioTracer = SIOTracer::GetInstance();
	{
		RCPtr<FileTracer> tracer(new FileTracer(stderr));
		sioTracer->AddTracer(tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceInfo, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceDebug, true, tracer);
	}

	RCPtr<AtrMemoryImage> atrImage(new AtrMemoryImage);

	if (!atrImage->ReadImageFromFile(argv[1])) {
		printf("reading input file \"%s\" failed\n", argv[1]);
		sioTracer->RemoveAllTracers();
		return 1;
	}

	RCPtr<AtpImage> atpImage = AtpUtils::CreateAtpImageFromAtrImage(atrImage);

	if (!atpImage) {
		printf("can not convert disk image into ATP format\n");
		sioTracer->RemoveAllTracers();
		return 1;
	} 

	if (!atpImage->WriteImageToFile(argv[2])) {
		printf("error writing ATP image to \"%s\"\n", argv[2]);
		sioTracer->RemoveAllTracers();
		return 1;
	}
	printf("successfully wrote ATP image \"%s\"\n", argv[2]);

	sioTracer->RemoveAllTracers();
	return 0;
}
