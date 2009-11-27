/*
   AtpUtils.cpp - misc helper routines for the ATP format

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

#include "AtpUtils.h"
#include "SIOTracer.h"
#include "AtariDebug.h"
#include "Atari1050Model.h"

using namespace Atari1050Model;

RCPtr<AtpImage> AtpUtils::CreateAtpImageFromAtrImage(RCPtr<AtrImage> atrImage)
{
	EDiskFormat format = atrImage->GetDiskFormat();

	switch (format) {
	case e90kDisk: {
		RCPtr<AtpImage> atpImage = new AtpImage;
		atpImage->SetNumberOfTracks(40);
		atpImage->SetDensity(eDensityFM);

		for (unsigned int track=0;track<40;track++) {
			for (unsigned int sector=1;sector<=18;sector++) {
				unsigned int position = CalculatePositionOfSDSector(
					track, sector);
				uint8_t buf[128];
				if (!atrImage->ReadSector(track*18+sector,buf,128)) {
					DPRINTF("error accessing internal ATR image!");
					return RCPtr<AtpImage>();
				}
				atpImage->AddSector(track, new AtpSector(
					sector, 128, buf, position,
					eSDSectorTimeLength));
			}
		}
		return atpImage;
	}
	case e130kDisk: {
		RCPtr<AtpImage> atpImage = new AtpImage;
		atpImage->SetNumberOfTracks(40);
		atpImage->SetDensity(eDensityMFM);

		for (unsigned int track=0;track<40;track++) {
			for (unsigned int sector=1;sector<=26;sector++) {
				unsigned int position = CalculatePositionOfEDSector(
					track, sector);
				uint8_t buf[128];
				if (!atrImage->ReadSector(track*26+sector,buf,128)) {
					DPRINTF("error accessing internal ATR image!");
					return RCPtr<AtpImage>();
				}
				atpImage->AddSector(track, new AtpSector(
					sector, 128, buf, position,
					eEDSectorTimeLength));
			}
		}
		return atpImage;
	}
	default:
		AERROR("can only convert 90k and 130k images into ATP format");
		return RCPtr<AtpImage>();
	}
}

