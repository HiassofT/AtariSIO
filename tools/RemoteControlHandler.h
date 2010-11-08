#ifndef REMOTECONTROLHANDLER_H
#define REMOTECONTROLHANDLER_H

/*
   RemoteControlHandler - SIO commands for remote control of atariserver

   Copyright (C) 2005 Matthias Reichl <hias@horus.com>

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


#include <limits.h>
#include "AbstractSIOHandler.h"
#include "SIOTracer.h"
#include "DeviceManager.h"
#include "CursesFrontend.h"
#include "DataContainer.h"

class RemoteControlHandler : public AbstractSIOHandler {
public:
	enum EEOLConversion { eRaw, eLF, eCRLF };

	RemoteControlHandler(DeviceManager* manager, CursesFrontend* frontend);
	virtual ~RemoteControlHandler();

	virtual int ProcessCommandFrame(SIO_command_frame& frame, const RCPtr<SIOWrapper>& wrapper);

	virtual bool IsRemoteControlHandler() const;

	virtual RCPtr<DiskImage> GetDiskImage();
	virtual RCPtr<const DiskImage> GetConstDiskImage() const;
	virtual bool EnableHighSpeed(bool);
	virtual bool SetHighSpeedParameters(unsigned int pokeyDivisor, unsigned int baudrate);
	virtual bool EnableXF551Mode(bool on);
	virtual bool EnableStrictFormatChecking(bool on);

	enum ERemoteCommandStatus {
		eRemoteCommandOK = 1,
		eRemoteCommandError = 146
	};

	void AddResultString(const char* string);

private:
	inline bool ValidDriveNo(const char);
	inline DeviceManager::EDriveNumber GetDriveNo(const char);

	void ResetResult();
	bool ProcessMultipleCommands(const char* buf, int buflen, const RCPtr<SIOWrapper>& wrapper);
	bool ProcessCommand(const char*, const RCPtr<SIOWrapper>& wrapper);
	ERemoteCommandStatus fLastCommandStatus;

	RCPtr<DataContainer> fResult;

	DeviceManager* fDeviceManager;
	CursesFrontend* fCursesFrontend;
	SIOTracer* fTracer;
};

inline bool RemoteControlHandler::ValidDriveNo(const char c)
{
	return (c >= '1') && (c <='8');
}

inline DeviceManager::EDriveNumber RemoteControlHandler::GetDriveNo(const char c)
{
	return DeviceManager::EDriveNumber(c - '1' + DeviceManager::eMinDriveNumber);
}

#endif
