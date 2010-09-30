/*
   atpdump.cpp - dump structure of an ATP image

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

#include "AtpImage.h"
#include "SIOTracer.h"
#include "FileTracer.h"

#ifdef ALL_IN_ONE
int atpdump_main(int argc, char** argv)
#else
int main(int argc, char** argv)
#endif
{
	if (argc != 2) {
		printf("usage: atpdump atp-file\n");
		return 1;
	}

        SIOTracer* sioTracer = SIOTracer::GetInstance();
	{
		RCPtr<FileTracer> tracer(new FileTracer(stderr));
		sioTracer->AddTracer(tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceInfo, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceWarning, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceError, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceDebug, true, tracer);
	}

	RCPtr<AtpImage> atpImage(new AtpImage);
	if (!atpImage->ReadImageFromFile(argv[1])) {
		printf("reading input file \"%s\" failed\n", argv[1]);
		sioTracer->RemoveAllTracers();
		return 1;
	}

	atpImage->Dump(std::cout);

	sioTracer->RemoveAllTracers();
	return 0;
}
