#ifndef ABSTRACTSIOHANDLER_H
#define ABSTRACTSIOHANDLER_H

/*
   AbstractSIOHandler.h - a (almost pure) virtual base class for
   SIO handlers

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

#include <sys/types.h>
#include <stdint.h>
#include "../driver/atarisio.h"
#include "SIOWrapper.h"
#include "DiskImage.h"

#include "RefCounted.h"
#include "RCPtr.h"

class AbstractSIOHandler : public RefCounted {
public:
	AbstractSIOHandler();
	virtual ~AbstractSIOHandler() {}

	enum ECommandStatus {
		eCommandOK = 0,
		eWriteProtected = 1,
		eUnsupportedCommand = 2,
		eInvalidPercomConfig = 3,
		eIllegalSectorNumber = 4,
		eImageError = 5,
		eAtpSectorStatus = 6,
		eAtpWrongSpeed = 7,
		eExecError = 8,
		eWritePrinterError = 9,
		eRemoteControlError = 10
	};
	virtual int ProcessCommandFrame(SIO_command_frame& frame, const RCPtr<SIOWrapper>& wrapper) = 0;
	// returns:
	// something defined in ECommandStatus or other = internal error

	virtual bool EnableHighSpeed(bool on) = 0;
	virtual bool SetHighSpeedParameters(unsigned int pokeyDivisor, unsigned int baudrate) = 0;

	virtual bool EnableXF551Mode(bool on) = 0;
	virtual bool EnableStrictFormatChecking(bool on) = 0;

	virtual bool IsAtrSIOHandler() const;
	virtual bool IsAtpSIOHandler() const;
	virtual bool IsPrinterHandler() const;
	virtual bool IsRemoteControlHandler() const;

	virtual RCPtr<DiskImage> GetDiskImage() = 0;
	virtual RCPtr<const DiskImage> GetConstDiskImage() const = 0;

	virtual void ProcessDelayedTasks(bool isForced = false);

	inline void SetActive(bool active)
	{
		fIsActive = active;
	}

	inline bool IsActive() const
	{
		return fIsActive;
	}

private:
	bool fIsActive;
};

#endif
