#ifndef ATRSIOHANDLER_H
#define ATRSIOHANDLER_H

/*
   AtrSIOHandler.h - handles SIO commands, the image data is stored
   in an AtrImage object

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


#include "AbstractSIOHandler.h"
#include "AtrImage.h"
#include "SIOTracer.h"
#include "VirtualImageObserver.h"

class AtrSIOHandler : public AbstractSIOHandler {
public:
	AtrSIOHandler(const RCPtr<AtrImage>&);
	/*
	 * if takeOwnership is true, the SIO handler will free the
	 * image in the destructor!
	 */
	virtual ~AtrSIOHandler();
	virtual int ProcessCommandFrame(SIO_command_frame& frame, const RCPtr<SIOWrapper>& wrapper);

	virtual bool IsAtrSIOHandler() const;

	virtual bool EnableHighSpeed(bool on);
	virtual bool SetHighSpeedParameters(unsigned int pokeyDivisor, unsigned int baudrate);
	virtual bool EnableXF551Mode(bool on);
	virtual bool EnableStrictFormatChecking(bool on);

	virtual RCPtr<DiskImage> GetDiskImage();
	virtual RCPtr<const DiskImage> GetConstDiskImage() const;

	RCPtr<AtrImage> GetAtrImage();
	RCPtr<const AtrImage> GetConstAtrImage() const;

	inline void SetVirtualImageObserver(RCPtr<VirtualImageObserver> observer);
	inline RCPtr<const VirtualImageObserver> GetVirtualImageObserver() const;

private:
	RCPtr<AtrImage> fImage;

	AtrImageConfig fImageConfig; // holds current image config
	AtrImageConfig fFormatConfig;
	/* fFormatConfig holds the format sent by the percom put command */
	/* At the moment this new config is only used after a "format disk" */
	/* command has been issued. In the time after the "percom put" */
	/* and before the "format disk" command, the (old) image config */
	/* is used. This behaviour is necessary, because some programs */
	/* send an invalid percom block to the drive. */

	bool fEnableHighSpeed;
	bool fEnableXF551Mode;
	bool fStrictFormatChecking;
	uint8_t fSpeedByte;
	unsigned int fHighSpeedBaudrate;

	uint8_t fLastFDCStatus;

	SIOTracer* fTracer;

	RCPtr<VirtualImageObserver> fVirtualImageObserver;

	inline bool IsVirtualImage() const;

	bool VerifyPercomFormat(uint8_t tracks, uint8_t sides, uint16_t sectors, uint16_t seclen, uint32_t total_sectors) const;

	// static temporary (sector-) buffer, for all instances
	enum { eBufferSize = 8192 };
	static uint8_t fBuffer[eBufferSize];
};

inline RCPtr<DiskImage> AtrSIOHandler::GetDiskImage()
{
	return fImage;
}

inline RCPtr<const DiskImage> AtrSIOHandler::GetConstDiskImage() const
{
	return fImage;
}

inline RCPtr<AtrImage> AtrSIOHandler::GetAtrImage()
{
	return fImage;
}

inline RCPtr<const AtrImage> AtrSIOHandler::GetConstAtrImage() const
{
	return fImage;
}

inline void AtrSIOHandler::SetVirtualImageObserver(RCPtr<VirtualImageObserver> observer)
{
	fVirtualImageObserver = observer;
}

inline RCPtr<const VirtualImageObserver> AtrSIOHandler::GetVirtualImageObserver() const
{
	return fVirtualImageObserver;
}

inline bool AtrSIOHandler::IsVirtualImage() const
{
	return fVirtualImageObserver.IsNotNull();
}

#endif
