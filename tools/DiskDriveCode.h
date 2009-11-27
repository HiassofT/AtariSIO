#ifndef DISKDRIVECODE_H
#define DISKDRIVECODE_H

/*
   DiskDriveCode - 6502 code sent to speedy/happy 1050 disk drive

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

#include "SIOWrapper.h"

class DiskDriveCode {
public:

	DiskDriveCode(const RCPtr<SIOWrapper>& sio, bool useHappy = false);
	~DiskDriveCode();

	bool SendCodeToDrive();

	bool CheckTrackTiming();

	bool SetReferenceSector(int sector, int trackno);

	bool ScanTrack(int trackno, uint8_t* buf);

	// buffer layout of ScanTrack
	enum {
		eScanIDs = 0,
		eScanStatus = 0x20,
		eScanTime = 0x40,
		eScanCount = 0x60,
		eScanInitTime = 0x61,
		eScanRemainTime = 0x62
	};
	      
	bool SetupSectorRead(uint8_t* buf);

	// buffer layout of SetupSectorRead
	enum {
		eSetupReadIDs = 0,
		eSetupReadRefSec = 0x20,
		eSetupReadDelay = 0x40,
		eSetupReadThisTrack = 0x60,
		eSetupReadRefTrack = 0x61
	};

	// read 129 bytes (128 data + 1 byte sector status)
	bool ReadSector(int idx, uint8_t* buf);

private:
	RCPtr<SIOWrapper> fSIOWrapper;

	bool fIsHappy;
	int fBaseAddress;

	enum {
		eSpeedyBaseAddress = 0x8000,
		eHappyBaseAddress = 0x9000
	};

	enum {
		eCMDBase = 0x68,
		eCMDSetRef = eCMDBase + 1,
		eCMDScan = eCMDSetRef + 1,
		eCMDRead = eCMDScan + 1
	};
	enum {
		eCMDSetRefOffset = 0,
		eCMDScanOffset = 3,
		eCMDReadOffset = 6
	};

	enum {
		eSetupReadOffset = 0x400
       	};


#include "6502/speedycode.h"

	static uint8_t fSpeedyCode[eSpeedyCodeLength];
};

#endif
