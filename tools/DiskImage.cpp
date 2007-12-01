/*
   DiskImage.cpp - common base class for handling images

   Copyright (C) 2002-2004 Matthias Reichl <hias@horus.com>

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

#include <stdlib.h>
#include <string.h>

#include "DiskImage.h"
#include "SIOTracer.h"
#include "AtariDebug.h"

DiskImage::DiskImage()
	: fFilename(0),
	  fWriteProtect(false),
	  fChanged(false),
	  fIsVirtualImage(false)
{
}

DiskImage::~DiskImage()
{
	if (fFilename) {
		free(fFilename);
		fFilename=0;
	}
}

const char* DiskImage::GetFilename() const
{
	return fFilename;
}

void DiskImage::SetFilename(const char* filename)
{
	if (fFilename) {
		free(fFilename);
	}
	if (filename) {
		fFilename=strdup(filename);
	} else {
		fFilename=0;
	}
}

bool DiskImage::WriteBackImageToFile() const
{
	if (GetFilename()) {
		return WriteImageToFile(GetFilename());
	} else {
		return false;
	}
}

bool DiskImage::IsAtrImage() const
{
	return false;
}

bool DiskImage::IsAtpImage() const
{
	return false;
}
