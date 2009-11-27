/*
   HighSpeedCode.cpp - MyPicoDos file loader

   (c) 2004 Matthias Reichl <hias@horus.com>

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

#include <string.h>

#include "MyPicoDosCode.h"
#include "AtariDebug.h"

MyPicoDosCode* MyPicoDosCode::fInstance = 0;

#include "6502/mypicodoscode.c"

MyPicoDosCode::MyPicoDosCode()
{
}

MyPicoDosCode::~MyPicoDosCode()
{
}

bool MyPicoDosCode::GetMyPicoDosSector(unsigned int sector, uint8_t* buf, unsigned int buflen) const
{
	if (!SectorNumberOK(sector)) {
		return false;
	}
	if (buflen != 128) {
		return false;
	}
	memcpy(buf, fCode + (sector-1)*128, 128);
	return true;
}

bool MyPicoDosCode::GetBootCodeSector(unsigned int sector, uint8_t* buf, unsigned int buflen) const
{
	if ((sector < 1) || (sector > 3)) {
		return false;
	}
	if (buflen != 128) {
		return false;
	}
	memcpy(buf, fBootCode + (sector-1)*128, 128);
	return true;
}

bool MyPicoDosCode::WriteBootCodeToImage(RCPtr<DiskImage> img, bool autorun) const
{
	uint8_t buf[128];
	int i;
	for (i=1;i<=3;i++) {
        	bool ok;
		ok = GetBootCodeSector(i, buf, 128);
		Assert(ok);
		if (i == 1 && autorun) {
			buf[9] = 1;
		}
		ok = img->WriteSector(i, buf, 128);
		if (!ok) {
			return false;
		}
	}
	return true;
}

