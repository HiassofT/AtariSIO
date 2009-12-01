/*
   VirtualImageObserver - track write access to directory sectors

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

#include "VirtualImageObserver.h"
#include "Dos2xUtils.h"
#include "AtariDebug.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

VirtualImageObserver::VirtualImageObserver(RCPtr<AtrImage> image)
{
	Assert(image.IsNotNull());
	fImage = image.GetRealPointer();
	fNumberOfSectors = fImage->GetNumberOfSectors();
	Assert(fNumberOfSectors >= 720);
	fDirectoryObserver = new Dos2xUtils*[fNumberOfSectors];
	unsigned int i;
	for (i=0;i<fNumberOfSectors;i++) {
		fDirectoryObserver[i] = 0;
	}
}

VirtualImageObserver::~VirtualImageObserver()
{
	fRootDirObserver = 0;
	unsigned int i;
	for (i=0;i<fNumberOfSectors;i++) {
		if (fDirectoryObserver[i] != 0) {
			Assert(false);
		}
	}
	delete[] fDirectoryObserver;
}

void VirtualImageObserver::SetDirectoryObserver(unsigned int dirsec, Dos2xUtils* utils)
{
	Assert((dirsec > 0) && (dirsec + 7 <= fNumberOfSectors));
	unsigned int i;
	for (i=0;i<7;i++) {
		if (fDirectoryObserver[dirsec+i-1]) {
			Assert(false);
		}
		fDirectoryObserver[dirsec+i-1] = utils;
	}
}

void VirtualImageObserver::RemoveDirectoryObserver(unsigned int dirsec, Dos2xUtils* utils)
{
	Assert((dirsec > 0) && (dirsec + 7 <= fNumberOfSectors));
	unsigned int i;
	for (i=0;i<7;i++) {
		if (fDirectoryObserver[dirsec+i-1] != utils) {
			Assert(false);
		}
		fDirectoryObserver[dirsec+i-1] = 0;
	}
}

void VirtualImageObserver::SetRootDirectoryObserver(RCPtr<Dos2xUtils> root)
{
	fRootDirObserver = root;
}

RCPtr<const Dos2xUtils> VirtualImageObserver::GetRootDirectoryObserver() const
{
	return fRootDirObserver;
}

void VirtualImageObserver::IndicateBeforeSectorWrite(unsigned int sector)
{
	if (sector < 1 || sector > fNumberOfSectors) {
		DPRINTF("illegal sector number in IndicateBeforeSectorWrite: %d\n", sector);
	} else {
		if (fDirectoryObserver[sector-1]) {
			fBufferedSectorNumber = sector;
			fImage->ReadSector(sector, fOldSectorBuffer, fImage->GetSectorLength());
		}
	}
}

void VirtualImageObserver::IndicateAfterSectorWrite(unsigned int sector)
{
	if (sector < 1 || sector > fNumberOfSectors) {
		DPRINTF("illegal sector number in IndicateAfterSectorWrite: %d\n", sector);
	} else {
       		if (fDirectoryObserver[sector-1]) {
 			if (sector != fBufferedSectorNumber) {
				DPRINTF("sector number mismatch in IndicateAfterSectorWrite");
				return;
			}
			fImage->ReadSector(sector, fNewSectorBuffer, fImage->GetSectorLength());
			fDirectoryObserver[sector-1]->IndicateSectorWrite(sector);
		}
	}
}

