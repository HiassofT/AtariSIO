/*
   FileTracer.cpp - send trace output to file

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

#include "FileTracer.h"
#include "Error.h"

//#define TRACE_TIMESTAMPS

#ifdef TRACE_TIMESTAMPS
#include "MiscUtils.h"
static MiscUtils::TimestampType lastTraceTimestamp;
static MiscUtils::TimestampType currentTraceTimestamp;
static char traceTimestampString[100];
#endif

FileTracer::FileTracer(const char* filename)
	: fNeedCloseFile(true)
{
	fFile = fopen(filename,"w");
	if (fFile == 0) {
		throw FileOpenError(filename);
	}
#ifdef TRACE_TIMESTAMPS
	lastTraceTimestamp = MiscUtils::GetCurrentTime();
#endif
}

FileTracer::FileTracer(FILE* file)
	: fNeedCloseFile(false),
	  fFile(file)
{
#ifdef TRACE_TIMESTAMPS
	lastTraceTimestamp = MiscUtils::GetCurrentTime();
#endif
}

FileTracer::~FileTracer()
{
	if (fNeedCloseFile) {
		fclose(fFile);
	}
}

void FileTracer::ReallyStartTraceLine()
{
#ifdef TRACE_TIMESTAMPS
	currentTraceTimestamp = MiscUtils::GetCurrentTime();
	snprintf(traceTimestampString, 100, "[ %llu / +%10llu ] ", 
		currentTraceTimestamp,
		currentTraceTimestamp - lastTraceTimestamp);
	lastTraceTimestamp = currentTraceTimestamp;
	AddString(traceTimestampString);
#endif
}
