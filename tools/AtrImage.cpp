/*
   AtrImage.cpp - common base class for handling ATR (Atari 8bit disk-) images

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

#include "AtrImage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AtariDebug.h"
#include "SIOTracer.h"

AtrImage::AtrImage()
{
	Init();
}

AtrImage::~AtrImage()
{
}

void AtrImage::Init()
{
	fImageConfig.fDiskFormat = eNoDisk;
	fImageConfig.fSectorLength = e128BytesPerSector;
	fImageConfig.fNumberOfSectors = 0;
	fImageConfig.fSectorsPerTrack = 0;
	fImageConfig.fTracksPerSide = 0;
	fImageConfig.fSides = 0;
	fImageConfig.fImageSize = 0;
	SetWriteProtect(false);
}

bool AtrImage::SetFormat(EDiskFormat format)
{
	fImageConfig.fDiskFormat = format;
	switch(format) {
	case eNoDisk:
		Init();
		break;
	case e90kDisk:
		fImageConfig.fSectorLength=e128BytesPerSector;
		fImageConfig.fNumberOfSectors=720;
		fImageConfig.fSectorsPerTrack=18;
		fImageConfig.fTracksPerSide=40;
		fImageConfig.fSides=1;
		fImageConfig.CalculateImageSize();
		break;
	case e130kDisk:
		fImageConfig.fSectorLength=e128BytesPerSector;
		fImageConfig.fNumberOfSectors=1040;
		fImageConfig.fSectorsPerTrack=26;
		fImageConfig.fTracksPerSide=40;
		fImageConfig.fSides=1;
		fImageConfig.CalculateImageSize();
		break;
	case e180kDisk:
		fImageConfig.fSectorLength=e256BytesPerSector;
		fImageConfig.fNumberOfSectors=720;
		fImageConfig.fSectorsPerTrack=18;
		fImageConfig.fTracksPerSide=40;
		fImageConfig.fSides=1;
		fImageConfig.CalculateImageSize();
		break;
	case e360kDisk:
		fImageConfig.fSectorLength=e256BytesPerSector;
		fImageConfig.fNumberOfSectors=1440;
		fImageConfig.fSectorsPerTrack=18;
		fImageConfig.fTracksPerSide=40;
		fImageConfig.fSides=2;
		fImageConfig.CalculateImageSize();
		break;
	default:
		Init();
		return false;
		break;
	}
	return true;
}

bool AtrImage::SetFormat(ESectorLength density, uint32_t numberOfSectors)
{
	if (numberOfSectors == 0 || numberOfSectors >=65536) {
		Init();
		return false;
	}

	fImageConfig.fSectorLength = density;
	fImageConfig.fNumberOfSectors = numberOfSectors;
	fImageConfig.fDiskFormat = eUserDefDisk;

	fImageConfig.fSectorsPerTrack = numberOfSectors;
	fImageConfig.fTracksPerSide = 1;
	fImageConfig.fSides = 1;

	switch (density) {
	case e128BytesPerSector:
	case e256BytesPerSector:
		/*
		 * try to figure out an appropriate disk mapping
		 */
		switch (numberOfSectors) {
		case 720:
			fImageConfig.fSectorsPerTrack = 18;
			fImageConfig.fTracksPerSide = 40;
			fImageConfig.fSides = 1;
			break;
		case 1040:
			fImageConfig.fSectorsPerTrack = 26;
			fImageConfig.fTracksPerSide = 40;
			fImageConfig.fSides = 1;
			break;
		case 1440:
			fImageConfig.fSectorsPerTrack = 18;
			fImageConfig.fTracksPerSide = 40;
			fImageConfig.fSides = 2;
			break;
		case 2880:
			fImageConfig.fSectorsPerTrack = 18;
			fImageConfig.fTracksPerSide = 80;
			fImageConfig.fSides = 2;
			break;
		case 5760:
			fImageConfig.fSectorsPerTrack = 36;
			fImageConfig.fTracksPerSide = 80;
			fImageConfig.fSides = 2;
			break;
		default:
			break;
		}
		break;
	case e512BytesPerSector:
		switch (numberOfSectors) {
		case 720:
			fImageConfig.fSectorsPerTrack = 9;
			fImageConfig.fTracksPerSide = 80;
			fImageConfig.fSides = 1;
			break;
		case 1440:
			fImageConfig.fSectorsPerTrack = 9;
			fImageConfig.fTracksPerSide = 80;
			fImageConfig.fSides = 2;
			break;
		case 2880:
			fImageConfig.fSectorsPerTrack = 18;
			fImageConfig.fTracksPerSide = 80;
			fImageConfig.fSides = 2;
			break;
		}
		break;
	default: break;
	}
	fImageConfig.DetermineDiskFormatFromLayout();
	fImageConfig.CalculateImageSize();
	return true;
}
		
bool AtrImage::SetFormat(ESectorLength density, unsigned int sectors, unsigned int tracks, unsigned int sides)
{
	unsigned int secs = sectors * tracks * sides;
	if (secs == 0 || secs >= 65536) {
		Init();
		return false;
	}

	fImageConfig.fSectorLength = density;
	fImageConfig.fNumberOfSectors = secs;
	fImageConfig.fSectorsPerTrack = sectors;
	fImageConfig.fTracksPerSide = tracks;
	fImageConfig.fSides = sides;

	fImageConfig.fDiskFormat = eUserDefDisk;

	fImageConfig.DetermineDiskFormatFromLayout();

	fImageConfig.CalculateImageSize();

	return true;
}

bool AtrImage::SetFormatFromATRHeader(const uint8_t* hdr)
{
	ESectorLength density;
	unsigned int numberOfSectors;
	unsigned int sectSize;
	size_t imgSize;

	if ( (hdr[0] != 0x96) || (hdr[1] != 0x02) ) {
		goto failure;
	}
	sectSize = hdr[4] | (hdr[5] << 8);

	switch (sectSize) {
	case 128:
		density = e128BytesPerSector;
		break;
	case 256:
		density = e256BytesPerSector;
		break;
	case 512:
		density = e512BytesPerSector;
		break;
	case 1024:
		density = e1kPerSector;
		break;
	case 2048:
		density = e2kPerSector;
		break;
	case 4096:
		density = e4kPerSector;
		break;
	case 8192:
		density = e8kPerSector;
		break;
	default:
		AERROR("unsupported sector size %d", sectSize);
		goto failure;
	}
	
	imgSize = (hdr[2] | (hdr[3] << 8) | (hdr[6] << 16) | (hdr[7] << 24)) << 4;

	switch (density) {
	case e256BytesPerSector:
		if (imgSize <= 384) {
			goto failure;
		} else {
			if (imgSize == 256*720) {
				// workaround for SIO2PC DD ATRs having 384 extra bytes at the end
				numberOfSectors = 720;
			} else {
				imgSize += 384;
				if ((imgSize & 0xff) != 0) {
					AERROR("illegal DD image size %ld", (unsigned long) (imgSize - 384));
					goto failure;
				}
				numberOfSectors = imgSize / 256;
			}
		}
		break;
	case e128BytesPerSector:
	case e512BytesPerSector:
	case e1kPerSector:
	case e2kPerSector:
	case e4kPerSector:
	case e8kPerSector:
		if (imgSize % sectSize != 0) {
			AERROR("illegal DD image size %ld", (unsigned long) imgSize);
			goto failure;
		}
		numberOfSectors = imgSize / sectSize;
		break;
	default:
		numberOfSectors = 0;
	}

	if (hdr[8] & 0x20) {
		SetWriteProtect(true);
	} else {
		SetWriteProtect(false);
	}
	return SetFormat(density, numberOfSectors);

failure:
	Init();
	return false;
}

bool AtrImage::CreateATRHeaderFromFormat(uint8_t* hdr) const
{
	size_t parSize;

	memset(hdr, 0, 16);

	if (fImageConfig.fNumberOfSectors == 0) {
		return false;
	}


	hdr[0] = 0x96;
	hdr[1] = 0x02;
	parSize = fImageConfig.fImageSize >> 4;
	hdr[2] = parSize & 0xff;
	hdr[3] = (parSize>>8) & 0xff;
	hdr[6] = (parSize>>16) & 0xff;
	hdr[7] = (parSize>>24) & 0xff;
	
	if (IsWriteProtected()) {
		hdr[8] |= 0x20;
	}
	switch (fImageConfig.fSectorLength) {
	case e128BytesPerSector:
		hdr[4] = 0x80;
		hdr[5] = 0;
		break;
	case e256BytesPerSector:
		hdr[4] = 0;
		hdr[5] = 1;
		break;
	case e512BytesPerSector:
		hdr[4] = 0;
		hdr[5] = 2;
		break;
	case e1kPerSector:
		hdr[4] = 0;
		hdr[5] = 4;
		break;
	case e2kPerSector:
		hdr[4] = 0;
		hdr[5] = 8;
		break;
	case e4kPerSector:
		hdr[4] = 0;
		hdr[5] = 16;
		break;
	case e8kPerSector:
		hdr[4] = 0;
		hdr[5] = 32;
		break;
	}

	return true;
}

bool AtrImage::IsAtrMemoryImage() const
{
	return false;
}

bool AtrImage::IsAtrImage() const
{
	return true;
}

bool AtrImage::ReadImageFromFile(const char*, bool)
{
	AssertMsg(false,"implement ReadImageFromFile in derived class\n");
	return false;
}

bool AtrImage::WriteImageToFile(const char*) const
{
	AssertMsg(false,"implement WriteImageToFile in derived class\n");
	return false;
}

bool AtrImage::ReadSector(unsigned int /*sector*/, uint8_t* /*buffer*/, unsigned int /*buffer_length*/) const
{
	DPRINTF("implement ReadSector in subclass!");
	return false;
}

bool AtrImage::WriteSector(unsigned int /*sector*/, const uint8_t* /*buffer*/, unsigned int /*buffer_length*/)
{
	DPRINTF("implement WriteSector in subclass!");
	return false;
}
