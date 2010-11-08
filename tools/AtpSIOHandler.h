#ifndef ATPSIOHANDLER_H
#define ATPSIOHANDLER_H

/*
   AtpSIOHandler.h - handles SIO commands, the image data is stored
   in an AtpImage object

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


#include "AbstractSIOHandler.h"
#include "AtpImage.h"
#include "SIOTracer.h"
#include "MiscUtils.h"

class AtpSIOHandler : public AbstractSIOHandler {
public:
	AtpSIOHandler(const RCPtr<AtpImage>&);
	/*
	 * if takeOwnership is true, the SIO handler will free the
	 * image in the destructor!
	 */
	virtual ~AtpSIOHandler();
	virtual int ProcessCommandFrame(SIO_command_frame& frame, const RCPtr<SIOWrapper>& wrapper);

	virtual bool EnableHighSpeed(bool on);
	virtual bool SetHighSpeedParameters(unsigned int pokeyDivisor, unsigned int baudrate);
	virtual bool EnableXF551Mode(bool on);
	virtual bool EnableStrictFormatChecking(bool on);

	virtual bool IsAtpSIOHandler() const;

	RCPtr<DiskImage> GetDiskImage();
	RCPtr<const DiskImage> GetConstDiskImage() const;

	RCPtr<AtpImage> GetAtpImage();
	RCPtr<const AtpImage> GetConstAtpImage() const;

private:
	// convert sector number to TrackNumber/SectorID
	// return true on success or false if the sector number
	// is out of range
	bool SectorToTrackId(unsigned int secno, unsigned int& trackno, unsigned int& sectorid) const;

	// seek to given track, return time needed for that operation
	unsigned int SeekToTrack(unsigned int track);

	// return time to spin up motor (returns zero if motor is
	// already running)
	unsigned int SpinUpMotor(const MiscUtils::TimestampType& currentTime);

	RCPtr<AtpImage> fImage;

	Atari1050Model::EDiskDensity fCurrentDensity;
	unsigned int fCurrentTrack;

	uint8_t fLastFDCStatus;
	MiscUtils::TimestampType fLastDiskAccessTimestamp;

	SIOTracer* fTracer;
};

inline RCPtr<const DiskImage> AtpSIOHandler::GetConstDiskImage() const
{
	return fImage;
}

inline RCPtr<DiskImage> AtpSIOHandler::GetDiskImage()
{
	return fImage;
}

inline RCPtr<AtpImage> AtpSIOHandler::GetAtpImage()
{
	return fImage;
}

inline RCPtr<const AtpImage> AtpSIOHandler::GetConstAtpImage() const
{
	return fImage;
}

#endif
