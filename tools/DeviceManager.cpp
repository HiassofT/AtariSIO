/*
   DeviceManager.cpp - collection of helper routines to manage devices
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

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "OS.h"
#include "DeviceManager.h"
#include "AtrMemoryImage.h"
#include "AtrSIOHandler.h"
#ifdef ENABLE_ATP
#include "AtpImage.h"
#include "AtpSIOHandler.h"
#include "AtpUtils.h"
#endif
#include "PrinterHandler.h"
#include "Error.h"
#include "Dos2xUtils.h"
#include "AtrSearchPath.h"
#include "MyPicoDosCode.h"

#include "AtariDebug.h"

DeviceManager::DeviceManager(const char* devname)
        : fUseStrictFormatChecking(false),
	  fTapeSpeedPercent(100)
{
	fSIOWrapper = SIOWrapper::CreateSIOWrapper(devname);
	fSIOManager = new SIOManager(fSIOWrapper);
	if (!SetSioServerMode(SIOWrapper::eCommandLine_RI)) {
		throw ErrorObject("unable to activate SIO server mode");
	}

	fPokeyDivisor = ATARISIO_POKEY_DIVISOR_3XSIO;
	fHighspeedBaudrate = fSIOWrapper->GetBaudrateForPokeyDivisor(fPokeyDivisor);
	fSioTiming = fSIOWrapper->GetDefaultSioTiming();
	if (fHighspeedBaudrate == 0) {
		SetHighSpeedMode(false);
	} else {
		SetHighSpeedMode(true);
	}

	EnableXF551Mode(false);
}

DeviceManager::~DeviceManager()
{
}

bool DeviceManager::SetSioServerMode(SIOWrapper::ESIOServerCommandLine cmdLine)
{
	if (fSIOWrapper->SetSIOServerMode(cmdLine)) {
		DPRINTF("cannot init SIO server mode!");
		return false;
	}
	fCableType = cmdLine;
	return true;
}

RCPtr<AbstractSIOHandler> DeviceManager::GetSIOHandler(EDriveNumber driveno) const
{
	if (driveno == ePrinter) {
		return fSIOManager->GetHandler(eSIOPrinter);
	}
	if (driveno == eRemoteControl) {
		return fSIOManager->GetHandler(eSIORemoteControl);
	}
	if (!DriveNumberOK(driveno)) {
		return NULL;
	}
	return fSIOManager->GetHandler(eSIODriveBase+driveno);
}

RCPtr<const AbstractSIOHandler> DeviceManager::GetConstSIOHandler(EDriveNumber driveno) const
{
	if (driveno == ePrinter) {
		return fSIOManager->GetConstHandler(eSIOPrinter);
	}
	if (driveno == eRemoteControl) {
		return fSIOManager->GetConstHandler(eSIORemoteControl);
	}
	if (!DriveNumberOK(driveno)) {
		return NULL;
	}
	return fSIOManager->GetConstHandler(eSIODriveBase+driveno);
}

bool DeviceManager::DriveInUse(EDriveNumber driveno) const
{
	RCPtr<const AbstractSIOHandler> h = GetConstSIOHandler(driveno);
	return h.IsNotNull();
}

RCPtr<DiskImage> DeviceManager::LoadDiskImage(const char* filename, bool beQuiet)
{
	RCPtr<DiskImage> image;

	char absPath[PATH_MAX];
	char myFilename[PATH_MAX];
	bool foundFile = false;

	if (strchr(filename,DIR_SEPARATOR) == NULL) {
		AtrSearchPath* sp = AtrSearchPath::GetInstance();
		foundFile = sp->SearchForFile(filename, myFilename, PATH_MAX);
	}
        if (!foundFile) {
		strncpy(myFilename, filename, PATH_MAX-1);
		myFilename[PATH_MAX-1] = 0;
	}
	if (realpath(myFilename,absPath) == 0) {
		if (!beQuiet) {
			AERROR("cannot find \"%s\"", filename);
		}
		return image;
	}

#ifdef ENABLE_ATP
	int len = strlen(absPath);
	if ( (len > 4 && strcasecmp(absPath+len-4,".atp") == 0) ||
	     (len > 7 && strcasecmp(absPath+len-7,".atp.gz") == 0)) {
		RCPtr<AtpImage> img(new AtpImage);
		if (img->ReadImageFromFile(absPath, beQuiet)) {
			image = img;
		}
	} else {
#else
	if (1) {
#endif
		RCPtr<AtrMemoryImage> img(new AtrMemoryImage);
		if (img->ReadImageFromFile(absPath, beQuiet)) {
			image = img;
		}
	}

	if (image.IsNotNull() && (image->GetFilename() == 0)) {
		image->SetFilename(absPath);
	}

	return image;
}

bool DeviceManager::LoadDiskImage(EDriveNumber driveno, const char* filename, bool beQuiet, bool forceUnload)
{
	if (!DriveNumberOK(driveno)) {
		return false;
	}

	if (DriveInUse(driveno) && ! forceUnload) {
		if (!beQuiet) {
			AERROR("already loaded image into D%d: - unload first",driveno);
		}
		return false;
	}

	RCPtr<DiskImage> image = LoadDiskImage(filename, beQuiet);

	if (image.IsNull()) {
		return false;
	}

	RCPtr<AbstractSIOHandler> handler;

#ifdef ENABLE_ATP
	if (image->IsAtpImage()) {
		handler = new AtpSIOHandler(RCPtrStaticCast<AtpImage>(image));
#else
	if (0) {
#endif
	} else if (image->IsAtrImage()) {
		handler = new AtrSIOHandler(RCPtrStaticCast<AtrImage>(image));
		handler->EnableHighSpeed(fUseHighSpeed);
		handler->SetHighSpeedParameters(fPokeyDivisor, fHighspeedBaudrate);
		handler->EnableXF551Mode(fEnableXF551Mode);
		handler->EnableStrictFormatChecking(fUseStrictFormatChecking);
	} else {
		return false;
	}

	if (DriveInUse(driveno) && forceUnload) {
		if (!beQuiet) {
			ALOG("unloading D%d:", driveno);
		}
		UnloadDiskImage(driveno);
	}

	if (!fSIOManager->RegisterHandler(eSIODriveBase+driveno, handler)) {
		if (!beQuiet) {
			DPRINTF("cannot register device %d!",eSIODriveBase+driveno);
		}
		return false;
	}

	if (!beQuiet) {
		ALOG("loaded D%d: from \"%s\"", driveno, image->GetFilename());
	}
	return true;
}

bool DeviceManager::CreateVirtualDrive(
	EDriveNumber driveno, const char* path, ESectorLength density, unsigned int sectors,
	bool MyDosFormat, bool forceUnload)
{
	char absPath[PATH_MAX];

	if (!DriveNumberOK(driveno)) {
		return false;
	}

	if (DriveInUse(driveno) && !forceUnload) {
		AERROR("already loaded image into D%d: - unload first",driveno);
		return false;
	}

	if (realpath(path,absPath) == 0) {
		AERROR("cannot locate directory \"%s\"", path);
		return false;
	}

	struct stat statbuf;
	if (stat(absPath, &statbuf)) {
		AERROR("cannot stat \"%s\"", absPath);
		return false;
	}
	if (!S_ISDIR(statbuf.st_mode)) {
		AERROR("\"%s\" is not a directory", absPath);
		return false;
	}
	int len = strlen(absPath);
	if ((len > 0) && (absPath[len-1] != DIR_SEPARATOR)) {
		absPath[len] = DIR_SEPARATOR;
		absPath[len+1] = 0;
	}

	if (!CreateAtrMemoryImage(driveno, density, sectors, forceUnload)) {
		return false;
	}

	RCPtr<AtrMemoryImage> img = RCPtrStaticCast<AtrMemoryImage>(GetAtrImage(driveno));
	img->SetIsVirtualImage(true);
	RCPtr<VirtualImageObserver> observer;
	RCPtr<Dos2xUtils> rootdir;
	RCPtr<AtrSIOHandler> sioHandler;


	observer = new VirtualImageObserver(img);
	rootdir = new Dos2xUtils(img, absPath, observer);
	observer->SetRootDirectoryObserver(rootdir);

	img->SetFilename(absPath);
	bool ok;
	if (MyDosFormat) {
		ok = rootdir->SetDosFormat(Dos2xUtils::eMyDos);
	} else {
		ok = rootdir->SetDosFormat(Dos2xUtils::eDos2x);
	}
	if (!ok) {
		AERROR("unable to set DOS format");
		goto fail;
	}
	if (!rootdir->InitVTOC()) {
		AERROR("unable to blank-init disk");
		goto fail;
	}

	if (!MyPicoDosCode::GetInstance()->WriteBootCodeToImage(img)) {
		AERROR("unable to write MyPicoDos boot sector code to image");
		goto fail;
	}

	//rootdir->AddFiles(false);
	rootdir->AddFiles(Dos2xUtils::ePicoName);

	sioHandler = RCPtrStaticCast<AtrSIOHandler>(GetSIOHandler(driveno));
	sioHandler->SetVirtualImageObserver(observer);
	return true;

fail:
	UnloadDiskImage(driveno);
	return false;
}

bool DeviceManager::CreateVirtualDrive(
	EDriveNumber driveno, const char* path, EDiskFormat format, bool forceUnload)
{
	// bool isMyDos = false;
	switch (format)
	{
	case e90kDisk:
		return CreateVirtualDrive(driveno, path, e128BytesPerSector, 720, false, forceUnload);
	case e130kDisk:
		return CreateVirtualDrive(driveno, path, e128BytesPerSector, 1040, false, forceUnload);
	case e180kDisk:
		return CreateVirtualDrive(driveno, path, e256BytesPerSector, 720, false, forceUnload);
	case e360kDisk:
		return CreateVirtualDrive(driveno, path, e256BytesPerSector, 1440, true, forceUnload);
	default:
		AERROR("illegal format in CreateVirtualDrive");
		return false;
	}
	Assert(false);
	return false;
}

bool DeviceManager::ReloadDrive(EDriveNumber driveno)
{
	int ok=true;

	int min, max;
	min = max = driveno;

	if (driveno == eAllDrives) {
		min = eMinDriveNumber;
		max = eMaxDriveNumber;
	} else {
		if (!DriveNumberOK(driveno)) {
			return false;
		}
	}

	for (int i=min; i<=max; i++) {
		driveno = (EDriveNumber) i;
		if (DriveInUse(EDriveNumber(i))) {
			RCPtr<AbstractSIOHandler> absHandler = GetSIOHandler(driveno);
			RCPtr<DiskImage> diskImage = absHandler->GetDiskImage();
			if (!diskImage->IsVirtualImage()) {
				if (diskImage->GetFilename() && (diskImage->GetFilename()[0] != 0)) {
					if (DriveIsChanged(driveno)) {
						ALOG("Not reloading changed drive D%d:", driveno);
					} else {
						ALOG("Reloading drive D%d:", driveno);

						char* filename = strdup(diskImage->GetFilename());

						bool active = DeviceIsActive(driveno);
						bool wp = DriveIsWriteProtected(driveno);
						UnloadDiskImage(driveno);

						if (!LoadDiskImage(driveno, filename, true)) {
							ALOG("ERROR reloading drive D%d:", driveno);
							ok = false;
						} else {
							SetDeviceActive(driveno, active);
							SetWriteProtectImage(driveno, wp);
						}
						free(filename);
					}
				}
			} else {

				ALOG("Reloading virtual drive D%d:", driveno);

				char* path;
				EDiskFormat diskFormat;
				ESectorLength seclen;
				unsigned int sectors;
				Dos2xUtils::EDosFormat dosFormat;

				{
					RCPtr<AbstractSIOHandler> absHandler = GetSIOHandler(driveno);
					if (!absHandler->IsAtrSIOHandler()) {
						Assert(false);
						return false;
					}
					RCPtr<AtrSIOHandler> atrHandler = RCPtrStaticCast<AtrSIOHandler>(absHandler);
					RCPtr<DiskImage> diskImage = atrHandler->GetDiskImage();
					RCPtr<const VirtualImageObserver> observer;
					RCPtr<const Dos2xUtils> rootdir;
					if (!diskImage->IsVirtualImage()) {
						return false;
					}

					RCPtr<AtrImage> atrImage = RCPtrStaticCast<AtrImage>(diskImage);
					observer = atrHandler->GetVirtualImageObserver();
					rootdir = observer->GetRootDirectoryObserver();

					diskFormat = atrImage->GetDiskFormat();
					path = strdup(diskImage->GetFilename());
					seclen = diskImage->GetSectorLength();
					sectors = diskImage->GetNumberOfSectors();

					dosFormat = rootdir->GetDosFormat();
				}

				// hack to create a MyDos disk
				if (dosFormat == Dos2xUtils::eMyDos) {
					diskFormat = eUserDefDisk;
				}


				UnloadDiskImage(driveno);
				if (diskFormat == eUserDefDisk) {
					unsigned int estimatedSectors = Dos2xUtils::EstimateDiskSize(path, seclen, Dos2xUtils::ePicoName);
					if (estimatedSectors > sectors) {
						sectors = estimatedSectors;
					}
					ok = CreateVirtualDrive(driveno, path, seclen, sectors);
				} else {
					ok = CreateVirtualDrive(driveno, path, diskFormat);
				}
				free(path);
			}
		}
	}
	return ok;
}

bool DeviceManager::CreateAtrMemoryImage(EDriveNumber driveno, EDiskFormat format, bool forceUnload)
{
	if (!DriveNumberOK(driveno)) {
		return false;
	}

	if (DriveInUse(driveno) && !forceUnload) {
		AERROR("already loaded image into D%d: - unload first",driveno);
		return false;
	}

	RCPtr<AtrMemoryImage> img(new AtrMemoryImage);
	img->CreateImage(format);
	img->SetChanged(false);

	RCPtr<AtrSIOHandler> handler(new AtrSIOHandler(img));
	handler->EnableHighSpeed(fUseHighSpeed);
	handler->SetHighSpeedParameters(fPokeyDivisor, fHighspeedBaudrate);
	handler->EnableXF551Mode(fEnableXF551Mode);
	handler->EnableStrictFormatChecking(fUseStrictFormatChecking);

	if (DriveInUse(driveno) && forceUnload) {
		ALOG("unloading D%d:", driveno);
		UnloadDiskImage(driveno);
	}

	if (!fSIOManager->RegisterHandler(eSIODriveBase+driveno, handler)) {
		DPRINTF("cannot register device %d!",eSIODriveBase+driveno);
		return false;
	}

	return true;
}

bool DeviceManager::CreateAtrMemoryImage(EDriveNumber driveno, ESectorLength density, unsigned int sectors, bool forceUnload)
{
	if (!DriveNumberOK(driveno)) {
		return false;
	}

	if (DriveInUse(driveno) && !forceUnload) {
		AERROR("already loaded image into D%d: - unload first",driveno);
		return false;
	}

	RCPtr<AtrMemoryImage> img(new AtrMemoryImage);
	img->CreateImage(density, sectors);
	img->SetChanged(false);

	RCPtr<AtrSIOHandler> handler(new AtrSIOHandler(img));
	handler->EnableHighSpeed(fUseHighSpeed);
	handler->SetHighSpeedParameters(fPokeyDivisor, fHighspeedBaudrate);
	handler->EnableXF551Mode(fEnableXF551Mode);
	handler->EnableStrictFormatChecking(fUseStrictFormatChecking);

	if (DriveInUse(driveno) && forceUnload) {
		ALOG("unloading D%d:", driveno);
		UnloadDiskImage(driveno);
	}

	if (!fSIOManager->RegisterHandler(eSIODriveBase+driveno, handler)) {
		DPRINTF("cannot register device %d!",eSIODriveBase+driveno);
		return false;
	}

	return true;
}

bool DeviceManager::UnloadDiskImage(EDriveNumber driveno)
{
	int min, max;

	if (driveno == eAllDrives) {
		min = eMinDriveNumber;
		max = eMaxDriveNumber;
	} else {
		if (!DriveNumberOK(driveno)) {
			return false;
		}
		min = driveno;
		max = driveno;
	}

	for (int i=min; i<=max;i++) {
		if (DriveInUse(EDriveNumber(i))) {
			fSIOManager->UnregisterHandler(eSIODriveBase+i);
		}
	}
	return true;
}

bool DeviceManager::SetDeviceActive(EDriveNumber driveno, bool on)
{
	int min, max;
	min = max = driveno;

	if (driveno == eAllDrives) {
		min = eMinDriveNumber;
		max = eMaxDriveNumber;
	} else {
		switch (driveno) {
		case ePrinter:
			if (!DriveInUse(driveno)) {
				DPRINTF("no printer handler installed");
				return false;
			}
			break;
		case eRemoteControl:
			if (!DriveInUse(driveno)) {
				DPRINTF("no remote control handler installed");
				return false;
			}
			break;
		default:
			if (!DriveNumberOK(driveno)) {
				return false;
			}
			if (!DriveInUse(driveno)) {
				DPRINTF("drive D%d: is empty",driveno);
				return false;
			}
			break;
		}
	}

	for (int i=min; i<=max;i++) {
		if (DriveInUse(EDriveNumber(i))) {
			RCPtr<AbstractSIOHandler> absHandler = GetSIOHandler((EDriveNumber)i);

			if (absHandler) {
				absHandler->SetActive(on);
			}
		}
	}
	return true;
}

bool DeviceManager::DeviceIsActive(EDriveNumber driveno) const
{
	RCPtr<const AbstractSIOHandler> absHandler = GetConstSIOHandler(driveno);
	if (absHandler.IsNotNull()) {
		return absHandler->IsActive();
	} else {
		return false;
	}
}

bool DeviceManager::SetWriteProtectImage(EDriveNumber driveno, bool on)
{
	int min, max;

	if (driveno == eAllDrives) {
		min = eMinDriveNumber;
		max = eMaxDriveNumber;
	} else {
		if (!DriveNumberOK(driveno)) {
			return false;
		}
		if (!DriveInUse(driveno)) {
			DPRINTF("drive D%d: is empty",driveno);
			return false;
		}
		min = driveno;
		max = driveno;
	}

	for (int i=min; i<=max;i++) {
		if (DriveInUse(EDriveNumber(i))) {
			RCPtr<AbstractSIOHandler> absHandler = GetSIOHandler((EDriveNumber)i);
			RCPtr<DiskImage> image = absHandler->GetDiskImage();

			if (image) {
				image->SetWriteProtect(on);
			}
		}
	}
	return true;
}

bool DeviceManager::WriteBackImage(EDriveNumber driveno)
{
	int i;
	int ok=true;

	if (driveno == eAllDrives) {
		for (i=eMinDriveNumber;i<=eMaxDriveNumber;i++) {
			if (DriveInUse(EDriveNumber(i))) {
				RCPtr<AbstractSIOHandler> absHandler = GetSIOHandler((EDriveNumber)i);
				RCPtr<DiskImage> diskImage = absHandler->GetDiskImage();
				if (!diskImage->IsVirtualImage() && diskImage->GetFilename() && diskImage->Changed()) {
					if (!diskImage->WriteBackImageToFile()) {
						ALOG("ERROR writing D%d: to \"%s\"", i, diskImage->GetFilename());
						ok = false;
					} else {
						ALOG("wrote D%d: to \"%s\"", i, diskImage->GetFilename());
					}
				}
			}
		}
	} else {
		if (!DriveNumberOK(driveno)) {
			return false;
		}
		if (!DriveInUse(driveno)) {
			return false;
		}
		RCPtr<AbstractSIOHandler> absHandler = GetSIOHandler(driveno);
		RCPtr<DiskImage> diskImage = absHandler->GetDiskImage();
		if (diskImage->IsVirtualImage() || (!diskImage->GetFilename())) {
			return false;
		}
		if (!diskImage->WriteBackImageToFile()) {
			return false;
		}
	}
	return ok;
}

bool DeviceManager::WriteBackImagesIfChanged()
{
	int i;
	int ok=true;
	for (i=eMinDriveNumber;i<=eMaxDriveNumber;i++) {
		if (DriveInUse(EDriveNumber(i))) {
			RCPtr<AbstractSIOHandler> absHandler = GetSIOHandler((EDriveNumber)i);
			RCPtr<DiskImage> diskImage = absHandler->GetDiskImage();
			if (!diskImage->IsVirtualImage() && diskImage->GetFilename() && diskImage->Changed()) {
				if (!diskImage->WriteBackImageToFile()) {
					ALOG("ERROR writing D%d: to \"%s\"", i, diskImage->GetFilename());
					ok = false;
				} else {
					ALOG("wrote D%d: to \"%s\"", i, diskImage->GetFilename());
				}
			}
		}
	}
	return ok;
}

bool DeviceManager::WriteDiskImage(EDriveNumber driveno, const char* filename)
{
	if (!DriveNumberOK(driveno)) {
		return false;
	}
	if (!DriveInUse(driveno)) {
		DPRINTF("drive D%d: is empty",driveno);
		return false;
	}

	if (!filename) {
		DPRINTF("filename is NULL!");
		return false;
	}
	
	RCPtr<AbstractSIOHandler> absHandler = GetSIOHandler(driveno);

	if (absHandler) {
		RCPtr<DiskImage> diskImage = absHandler->GetDiskImage();

		bool ok;
#ifdef ENABLE_ATP
		int len = strlen(filename);
		if ( (len > 4 && strcasecmp(filename+len-4,".atp") == 0) ||
		     (len > 7 && strcasecmp(filename+len-7,".atp.gz") == 0)) {
			if (diskImage->IsAtrImage()) {
				ok = false;
				RCPtr<FileIO> fileio;
				RCPtr<AtrImage> atrImage = RCPtrStaticCast<AtrImage>(diskImage);
				RCPtr<AtpImage> atpImage = AtpUtils::CreateAtpImageFromAtrImage(atrImage);
				if (!atpImage) {
					goto error;
				}
				if (len > 7 && strcasecmp(filename+len-7,".atp.gz")) {
#ifdef USE_ZLIB
					fileio = new GZFileIO();
#else
					AERROR("cannot write compressed file - zlib support disabled at compiletime!");
					goto error;
#endif
				} else {
					fileio = new StdFileIO();
				}
				ok = atpImage->WriteImageToFile(filename);
				if (ok) {
					diskImage->SetChanged(false);
				}
			} else {
				ok=diskImage->WriteImageToFile(filename);
			}
			if (ok && !diskImage->IsAtpImage()) {
				ALOG("changed image type to ATP - reloading drive D%d:",driveno);
				UnloadDiskImage(driveno);
				ok = LoadDiskImage(driveno, filename);
			}
		} else {
			if (diskImage->IsAtpImage()) {
				AERROR("cannot convert ATP image into other image types!");
				return false;
			}
#else
		if (1) {
#endif
			ok=diskImage->WriteImageToFile(filename);
		}

#ifdef ENABLE_ATP
error:
#endif
		if (!ok) {
			return false;
		} else {
			if (!diskImage->IsVirtualImage()) {
				char absPath[PATH_MAX];
				if (realpath(filename,absPath) == 0) {
					DPRINTF("get realpath failed");
					return false;
				}
				diskImage->SetFilename(absPath);
				ALOG("wrote D%d: to \"%s\"", driveno, diskImage->GetFilename());
			} else {
				ALOG("wrote D%d: to \"%s\"", driveno, filename);
			}
		}
	} else {
		DPRINTF("internal error - can't get SIO handler!");
		return false;
	}
	return true;
}

bool DeviceManager::ExchangeDrives(EDriveNumber drive1, EDriveNumber drive2)
{
	if (!DriveNumberOK(drive1) || !DriveNumberOK(drive2)) {
		return false;
	}
	if (drive1 == drive2) {
		DPRINTF("exchanging a drive with itself is not allowed!");
		return false;
	}
	RCPtr<AbstractSIOHandler> h1 (fSIOManager->GetHandler(eSIODriveBase+drive1));
	RCPtr<AbstractSIOHandler> h2 (fSIOManager->GetHandler(eSIODriveBase+drive2));

	fSIOManager->UnregisterHandler(eSIODriveBase+drive1);
	fSIOManager->UnregisterHandler(eSIODriveBase+drive2);

	fSIOManager->RegisterHandler(eSIODriveBase+drive1,h2);
	fSIOManager->RegisterHandler(eSIODriveBase+drive2,h1);
	return true;
}

const char* DeviceManager::GetImageFilename(EDriveNumber driveno) const
{
	if (!DriveNumberOK(driveno)) {
		return 0;
	}
	if (DriveInUse(driveno)) {
		RCPtr<const DiskImage> image = GetConstSIOHandler(driveno)->GetConstDiskImage();
		if (image) {
			return image->GetFilename();
		}
	}
	return 0;
}

bool DeviceManager::DriveIsWriteProtected(EDriveNumber driveno) const
{
	if (!DriveNumberOK(driveno)) {
		return false;
	}
	if (DriveInUse(driveno)) {
		RCPtr<const DiskImage> image = GetConstSIOHandler(driveno)->GetConstDiskImage();

		if (image) {
			return image->IsWriteProtected();
		}
	}
	return false;
}

bool DeviceManager::DriveIsChanged(EDriveNumber driveno) const
{
	int min, max;

	if (driveno == eAllDrives) {
		min = eMinDriveNumber;
		max = eMaxDriveNumber;
	} else {
		if (!DriveNumberOK(driveno)) {
			return false;
		}
		if (!DriveInUse(driveno)) {
			DPRINTF("drive D%d: is empty",driveno);
			return false;
		}
		min = driveno;
		max = driveno;
	}
	for (int i=min; i<=max; i++) {
		if (DriveInUse( (EDriveNumber) i ) ) {
			RCPtr<const DiskImage> img = GetConstSIOHandler((EDriveNumber) i)->GetConstDiskImage();
			if (img.IsNull()) {
				DPRINTF("got null image in DriveIsChanged()");
			} else {
				if (!img->IsVirtualImage() && img->Changed()) {
					return true;
				}
			}
		}
	}
	return false;
}

bool DeviceManager::DriveIsVirtualImage(EDriveNumber driveno) const
{
	if (!DriveNumberOK(driveno)) {
		return false;
	}
	if (!DriveInUse(driveno)) {
		DPRINTF("drive D%d: is empty",driveno);
		return false;
	}
	RCPtr<const DiskImage> img = GetConstSIOHandler(driveno)->GetConstDiskImage();
	if (img.IsNull()) {
		DPRINTF("got null image in DriveIsVirtualImage()");
	} else {
		if (img->IsVirtualImage()) {
			return true;
		}
	}
	return false;
}

unsigned int DeviceManager::GetDriveImageSize(EDriveNumber driveno) const
{
	if (!DriveNumberOK(driveno)) {
		return 0;
	}
	if (DriveInUse(driveno)) {
		RCPtr<const AbstractSIOHandler> absHandler = GetConstSIOHandler(driveno);

		if (absHandler->IsAtrSIOHandler()) {
			RCPtr<const AtrSIOHandler> handler = RCPtrStaticCast<const AtrSIOHandler>(absHandler);
			return handler->GetConstAtrImage()->GetImageSize();
		}
	}
	return 0;
}

int DeviceManager::DoServing(int otherReadPollDevice)
{
	return fSIOManager->DoServing(otherReadPollDevice);
}

bool DeviceManager::DriveNumberOK(EDriveNumber driveno) const
{
	if (driveno < eMinDriveNumber || driveno > eMaxDriveNumber ) {
		DPRINTF("illegal drive number %d",driveno);
		return false;
	}
	return true;
}

bool DeviceManager::SetHighSpeedMode(bool on)
{
	if (on) {
		if (fSIOWrapper->SetBaudrate(fHighspeedBaudrate)) {
			return false;
		}
		if (fSIOWrapper->SetAutobaud(1)) {
			return false;
		}
	} else {
		if (fSIOWrapper->SetBaudrate(fSIOWrapper->GetStandardBaudrate())) {
			return false;
		}
		if (fSIOWrapper->SetAutobaud(0)) {
			return false;
		}
	}
	fUseHighSpeed = on;

	for (int i=eMinDriveNumber; i<=eMaxDriveNumber; i++) {
		if (DriveInUse(EDriveNumber(i))) {
			GetSIOHandler((EDriveNumber)i)->SetHighSpeedParameters(fPokeyDivisor, fHighspeedBaudrate);
			GetSIOHandler((EDriveNumber)i)->EnableHighSpeed(fUseHighSpeed);
		}
	}

	fSIOWrapper->SetBaudrate(fSIOWrapper->GetStandardBaudrate());

	return true;
}

bool DeviceManager::SetSioTiming(SIOWrapper::ESIOTiming timing)
{
	if (fSIOWrapper->SetSioTiming(timing)) {
		AERROR("cannot set SIO timing");
		return false;
	}
	fSioTiming = timing;
	return true;
}

bool DeviceManager::SetHighSpeedParameters(unsigned int pokeyDivisor, unsigned int baudrate)
{
	if (pokeyDivisor >= 64) {
		AERROR("illegal high speed pokey divisor %d", pokeyDivisor);
		return false;
	}
	if (baudrate == 0) {
		baudrate = fSIOWrapper->GetBaudrateForPokeyDivisor(pokeyDivisor);
		if (baudrate == 0) {
			AERROR("pokey divisor %d is not supported by driver", pokeyDivisor);
			return false;
		}
	}

	fHighspeedBaudrate = baudrate;
	fPokeyDivisor = pokeyDivisor;
	fSIOWrapper->SetHighSpeedBaudrate(baudrate);

	if (fUseHighSpeed) {
		fSIOWrapper->SetBaudrate(baudrate);
		int real_baudrate = fSIOWrapper->GetExactBaudrate();

		if (real_baudrate != (int)baudrate) {
			AWARN("UART doesn't support %d baud, using %d instead", baudrate, real_baudrate);
		}
	}
	return SetHighSpeedMode(fUseHighSpeed);
}

bool DeviceManager::EnableXF551Mode(bool on)
{
	fEnableXF551Mode = on;
	for (int i=eMinDriveNumber; i<=eMaxDriveNumber; i++) {
		if (DriveInUse(EDriveNumber(i))) {
			GetSIOHandler((EDriveNumber)i)->EnableXF551Mode(on);
		}
	}
	return true;
}

bool DeviceManager::EnableStrictFormatChecking(bool on)
{
	fUseStrictFormatChecking = on;
	for (int i=eMinDriveNumber; i<=eMaxDriveNumber; i++) {
		if (DriveInUse(EDriveNumber(i))) {
			GetSIOHandler((EDriveNumber)i)->EnableStrictFormatChecking(on);
		}
	}
	return true;
}


RCPtr<AtrImage> DeviceManager::GetAtrImage(EDriveNumber driveno)
{
	if (!DriveNumberOK(driveno)) {
		return RCPtr<AtrImage>();
	}
	RCPtr<AbstractSIOHandler> handler(GetSIOHandler(driveno));
	if (handler && handler->IsAtrSIOHandler()) {
		RCPtr<AtrSIOHandler> atrHandler = RCPtrStaticCast<AtrSIOHandler>(handler);
		return atrHandler->GetAtrImage();
	}
	return RCPtr<AtrImage>();
}

RCPtr<const AtrImage> DeviceManager::GetConstAtrImage(EDriveNumber driveno) const
{
	if (!DriveNumberOK(driveno)) {
		return RCPtr<AtrImage>();
	}
	RCPtr<const AbstractSIOHandler> handler(GetConstSIOHandler(driveno));
	if (handler && handler->IsAtrSIOHandler()) {
		RCPtr<const AtrSIOHandler> atrHandler(RCPtrStaticCast<const AtrSIOHandler> (handler));
		return atrHandler->GetConstAtrImage();
	}
	return RCPtr<AtrImage>();
}

RCPtr<DiskImage> DeviceManager::GetDiskImage(EDriveNumber driveno)
{
	if (!DriveNumberOK(driveno)) {
		return RCPtr<DiskImage>();
	}
	RCPtr<AbstractSIOHandler> handler(GetSIOHandler(driveno));
	if (handler) {
		return handler->GetDiskImage();
	}
	return RCPtr<DiskImage>();
}

RCPtr<const DiskImage> DeviceManager::GetConstDiskImage(EDriveNumber driveno) const
{
	if (!DriveNumberOK(driveno)) {
		return RCPtr<DiskImage>();
	}
	RCPtr<const AbstractSIOHandler> handler(GetConstSIOHandler(driveno));
	if (handler) {
		return handler->GetConstDiskImage();
	}
	return RCPtr<DiskImage>();
}

bool DeviceManager::CheckForChangedImages()
{
	return DriveIsChanged(eAllDrives);
}

bool DeviceManager::InstallPrinterHandler(const char* dest, PrinterHandler::EEOLConversion conv)
{
	if (DriveInUse(ePrinter)) {
		DPRINTF("printer handler already installed");
		return false;
	}
	try {
		RCPtr<PrinterHandler> handler = new PrinterHandler(dest, conv);
		if (!fSIOManager->RegisterHandler(eSIOPrinter, handler)) {
			DPRINTF("cannot register printer handler");
			return false;
		}
	} catch (ErrorObject& err) {
		AERROR("%s", err.AsCString());
		return false;
	}
	ALOG("installed printer handler");
	return true;
}

bool DeviceManager::RemovePrinterHandler()
{
	if (!DriveInUse(ePrinter)) {
		DPRINTF("printer handler is not installed");
		return false;
	}
	fSIOManager->UnregisterHandler(eSIOPrinter);
	ALOG("removed printer handler");
	return true;
}

bool DeviceManager::FlushPrinterData()
{
	RCPtr<AbstractSIOHandler> handler = GetSIOHandler(ePrinter);
	if (handler.IsNotNull()) {
		handler->ProcessDelayedTasks(true);
		return true;
	} else {
		DPRINTF("flush printer data: no printer handler installed");
		return false;
	}
}

PrinterHandler::EEOLConversion DeviceManager::GetPrinterEOLConversion() const
{
	if (DriveInUse(ePrinter)) {
		RCPtr<const PrinterHandler> handler = RCPtrStaticCast<const PrinterHandler>(GetConstSIOHandler(ePrinter));
		if (handler.IsNull()) {
			Assert(false);
			return PrinterHandler::eRaw;
		} else {
			return handler->GetEOLConversion();
		}
	} else {
		Assert(false);
		return PrinterHandler::eRaw;
	}
}

PrinterHandler::EPrinterStatus DeviceManager::GetPrinterRunningStatus() const
{
	if (DriveInUse(ePrinter)) {
		RCPtr<const PrinterHandler> handler = RCPtrStaticCast<const PrinterHandler>(GetConstSIOHandler(ePrinter));
		if (handler.IsNull()) {
			Assert(false);
			return PrinterHandler::eStatusError;
		} else {
			return handler->GetPrinterStatus();
		}
	} else {
		Assert(false);
		return PrinterHandler::eStatusError;
	}
}

const char* DeviceManager::GetPrinterFilename() const
{
	if (DriveInUse(ePrinter)) {
		RCPtr<const PrinterHandler> handler = RCPtrStaticCast<const PrinterHandler>(GetConstSIOHandler(ePrinter));
		if (handler.IsNull()) {
			Assert(false);
			return NULL;
		} else {
			return handler->GetFilename();
		}
	} else {
		Assert(false);
		return NULL;
	}
}

void DeviceManager::UnloadCasImage()
{
	fCasHandler.SetToNull();
}	

bool DeviceManager::LoadCasImage(const char* filename)
{
	char absPath[PATH_MAX];
	UnloadCasImage();

	if (realpath(filename,absPath) == 0) {
		AERROR("cannot find \"%s\"", filename);
		return false;
	}

	RCPtr<CasImage> image = new CasImage;

	if (image->ReadImageFromFile(absPath)) {
		fCasHandler = new CasHandler(image, fSIOWrapper);
		fCasHandler->SetTapeSpeedPercent(fTapeSpeedPercent);
		return true;
	} else {
		return false;
	}
}

bool DeviceManager::SetTapeSpeedPercent(unsigned int p)
{
	if (p == 0 || p >= 200) {
		AERROR("illegal tape speed percent %d", p);
		return false;
	} else {
		fTapeSpeedPercent = p;
		if (fCasHandler) {
			fCasHandler->SetTapeSpeedPercent(fTapeSpeedPercent);
		}
		return true;
	}
}
