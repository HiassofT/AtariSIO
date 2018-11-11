#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

/*
   DeviceManager.h - collection of helper routines to manage devices
   and load/unload them into the SIO server (SIOManager)

   Copyright (C) 2003-2005 Matthias Reichl <hias@horus.com>

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

#include "SIOManager.h"
#include "AtrImage.h"
#include "RCPtr.h"
#include "RefCounted.h"
#include "PrinterHandler.h"
#include "CasHandler.h"

class DeviceManager : public RefCounted {
public:
	DeviceManager(const char* devname = 0);
	virtual ~DeviceManager();

	bool SetSioServerMode(SIOWrapper::ESIOServerCommandLine cmdLine);

	SIOWrapper::ESIOServerCommandLine GetSioServerMode() const;

	enum EDriveNumber {
		eNoDrive = -1,
		eAllDrives = 0,
		eDrive1 = 1,
		eDrive2 = 2,
		eDrive3 = 3,
		eDrive4 = 4,
		eDrive5 = 5,
		eDrive6 = 6,
		eDrive7 = 7,
		eDrive8 = 8,
		eMinDriveNumber = eDrive1,
		eMaxDriveNumber = eDrive8,
		ePrinter = 9,
		eRemoteControl = 10,
		eCassette = 11
	};
	enum {
		ePrinterPosition = (ePrinter - eMinDriveNumber)
	};

	enum {eSIODriveBase=0x30, eSIOPrinter = 0x40, eSIORemoteControl = 0x61 };

	// floppy disk functions:

	static RCPtr<DiskImage> LoadDiskImage(const char* filename, bool beQuiet = false);
	bool LoadDiskImage(EDriveNumber driveno, const char* filename, bool beQuiet = false, bool forceUnload = false);

	bool ReloadDrive(EDriveNumber driveno);

	bool CreateVirtualDrive(
		EDriveNumber driveno, const char* path, EDiskFormat format, bool forceUnload = false);
	bool CreateVirtualDrive(
		EDriveNumber driveno, const char* path, ESectorLength density, unsigned int sectors, bool MyDosFormat=true, bool forceUnload = false);

	bool CreateAtrMemoryImage(EDriveNumber driveno, EDiskFormat format, bool forceUnload = false);
	bool CreateAtrMemoryImage(EDriveNumber driveno, ESectorLength density, unsigned int sectors, bool forceUnload = false);
	bool WriteDiskImage(EDriveNumber driveno, const char* filename);
	bool UnloadDiskImage(EDriveNumber driveno);
	bool SetWriteProtectImage(EDriveNumber driveno, bool on);
	bool SetDeviceActive(EDriveNumber driveno, bool on);
	bool DeviceIsActive(EDriveNumber driveno) const;
	bool WriteBackImage(EDriveNumber driveno);
	bool WriteBackImagesIfChanged();
	const char* GetImageFilename(EDriveNumber driveno) const;
	bool ExchangeDrives(EDriveNumber drive1, EDriveNumber drive2);
	bool DriveInUse(EDriveNumber driveno) const;
	bool DriveIsWriteProtected(EDriveNumber driveno) const;
	bool DriveIsChanged(EDriveNumber driveno) const;
	bool DriveIsVirtualImage(EDriveNumber driveno) const;
	unsigned int GetDriveImageSize(EDriveNumber driveno) const;
	bool DriveNumberOK(EDriveNumber driveno) const;

	bool InstallPrinterHandler(const char* dest, PrinterHandler::EEOLConversion);
	bool RemovePrinterHandler();
	bool FlushPrinterData();

	PrinterHandler::EEOLConversion GetPrinterEOLConversion() const;
	PrinterHandler::EPrinterStatus GetPrinterRunningStatus() const;
	const char* GetPrinterFilename() const;

	bool SetHighSpeedMode(bool on);
	bool GetHighSpeedMode() const;

	bool SetSioTiming(SIOWrapper::ESIOTiming timing);
	SIOWrapper::ESIOTiming GetSioTiming() const;

	bool SetHighSpeedParameters(unsigned int pokeyDivsor, unsigned int baudrate);
	inline uint8_t GetHighSpeedPokeyDivisor() const;
	inline unsigned int GetHighSpeedBaudrate() const;

	bool EnableXF551Mode(bool on);
	bool GetXF551Mode() const;

	bool EnableStrictFormatChecking(bool on);
	bool GetStrictFormatChecking() const;

	int DoServing(int otherReadPollDevice=-1);

	RCPtr<SIOManager> GetSIOManager();
	RCPtr<SIOWrapper> GetSIOWrapper();

	RCPtr<AtrImage> GetAtrImage(EDriveNumber driveno);
	RCPtr<const AtrImage> GetConstAtrImage(EDriveNumber driveno) const;
	// may return NULL

	RCPtr<DiskImage> GetDiskImage(EDriveNumber driveno);
	RCPtr<const DiskImage> GetConstDiskImage(EDriveNumber driveno) const;

	// returns true if there are any changed images
	bool CheckForChangedImages();

	RCPtr<CasHandler> GetCasHandler();

	bool LoadCasImage(const char* filename);
	void UnloadCasImage();

	bool SetTapeSpeedPercent(unsigned int p);
	inline unsigned int GetTapeSpeedPercent() const;

private:
	RCPtr<SIOWrapper> fSIOWrapper;
	RCPtr<SIOManager> fSIOManager;

	RCPtr<AbstractSIOHandler> GetSIOHandler(EDriveNumber driveno) const;
	RCPtr<const AbstractSIOHandler> GetConstSIOHandler(EDriveNumber driveno) const;

	bool fUseHighSpeed;
	SIOWrapper::ESIOTiming fSioTiming;

	unsigned int fHighspeedBaudrate;
	unsigned int fPokeyDivisor;
	bool fUseStrictFormatChecking;

	unsigned int fTapeSpeedPercent;
	bool fEnableXF551Mode;
	SIOWrapper::ESIOServerCommandLine fCableType;
	RCPtr<CasHandler> fCasHandler;
};

inline RCPtr<SIOManager> DeviceManager::GetSIOManager()
{
	return fSIOManager;
}

inline RCPtr<SIOWrapper> DeviceManager::GetSIOWrapper()
{
	return fSIOWrapper;
}

inline RCPtr<CasHandler> DeviceManager::GetCasHandler()
{
	return fCasHandler;
}

inline SIOWrapper::ESIOServerCommandLine DeviceManager::GetSioServerMode() const
{
	return fCableType;
}

inline bool DeviceManager::GetHighSpeedMode() const
{
	return fUseHighSpeed;
}

inline SIOWrapper::ESIOTiming DeviceManager::GetSioTiming() const
{
	return fSioTiming;
}

inline bool DeviceManager::GetXF551Mode() const
{
	return fEnableXF551Mode;
}

inline bool DeviceManager::GetStrictFormatChecking() const
{
	return fUseStrictFormatChecking;
}

inline unsigned int DeviceManager::GetHighSpeedBaudrate() const
{
	return fHighspeedBaudrate;
}

inline uint8_t DeviceManager::GetHighSpeedPokeyDivisor() const
{
	return fPokeyDivisor;
}

inline unsigned int DeviceManager::GetTapeSpeedPercent() const
{
	return fTapeSpeedPercent;
}
#endif
