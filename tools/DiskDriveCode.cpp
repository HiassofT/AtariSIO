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

#include <stdio.h>

#include "DiskDriveCode.h"
#include "AtariDebug.h"

#include "6502/speedycode.c"

DiskDriveCode::DiskDriveCode(const RCPtr<SIOWrapper>& sio, bool isHappy)
	: fSIOWrapper(sio),
	  fIsHappy(isHappy)
{ 
	if (fIsHappy) {
		fBaseAddress = eHappyBaseAddress;
	} else {
		fBaseAddress = eSpeedyBaseAddress;
	}
}

DiskDriveCode::~DiskDriveCode()
{ }

extern int hexdump(void* buf, int len);

bool DiskDriveCode::SendCodeToDrive()
{
	int i;
	int ret;
	int adr;
	SIO_parameters params;
	uint8_t buf[256];

	// set drive slow
	params.device_id = 0x31;
	params.timeout = 7;
	params.direction = 1;
	params.data_buffer = buf;
	params.data_length = 0;
	params.command = 0x4b;
	params.aux1 = 3;

	ret = fSIOWrapper->DirectSIO(params);
	if (ret < 0) {
		AERROR("setting drive slow failed: %d\n", ret);
		return false;
	}

	params.command = 'P';
	params.data_length = 128;
	params.direction = 1;

	for (i=0;i<eSpeedyCodeLength;i+=128) {
		adr = fBaseAddress + i;
		params.aux1 = adr & 0xff;
		params.aux2 = adr >> 8;
		params.data_buffer = fSpeedyCode + i;
		ret = fSIOWrapper->DirectSIO(params);
		if (ret != 0) {
			AERROR("write ram[%d]: %d\n", i, ret);
			return false;
		}
	}

	/*
	params.command = 'R';
	params.direction = 0;
	params.data_buffer = buf;

	for (i=0;i<eSpeedyCodeLength;i+=128) {
		params.aux1 = i & 0xff;
		params.aux2 = 0x80+(i>>8);
		ret = fSIOWrapper->DirectSIO(params);
		if (ret != 0) {
			ALOG("reading ram[%d] failed: %d", i, ret);
			return false;
		}
		printf("ram[%d]\n", i);
		hexdump(buf, 128);
	}
	*/

	params.command = 0x41;
	params.data_buffer = buf;
	params.data_length = 3;
	params.direction = 1;

	// install command set reference sector
	adr = fBaseAddress + eCMDSetRefOffset;
	buf[0] = eCMDSetRef;
	buf[1] = adr & 0xff;
	buf[2] = adr >> 8;

	ret = fSIOWrapper->DirectSIO(params);
	if (ret != 0) {
		printf("adding command failed: %d\n", ret);
		return false;
	}

	// install command scan track
	adr = fBaseAddress + eCMDScanOffset;
	buf[0] = eCMDScan;
	buf[1] = adr & 0xff;
	buf[2] = adr >> 8;

	ret = fSIOWrapper->DirectSIO(params);
	if (ret != 0) {
		printf("adding command failed: %d\n", ret);
		return false;
	}

	// install command read sector
	adr = fBaseAddress + eCMDReadOffset;
	buf[0] = eCMDRead;
	buf[1] = adr & 0xff;
	buf[2] = adr >> 8;

	ret = fSIOWrapper->DirectSIO(params);
	if (ret != 0) {
		printf("adding command failed: %d\n", ret);
		return false;
	}

	return true;
}

bool DiskDriveCode::CheckTrackTiming()
{
	int ret;
	SIO_parameters params;
	uint8_t buf[256];

	params.device_id = 0x31;
	params.command = 'z';
	params.timeout = 7;
	params.data_length = 128;
	params.direction = 0;
	params.data_buffer = buf;
	params.aux1 = 42;
	params.aux2 = 23;

	ret = fSIOWrapper->DirectSIO(params);
	if (ret != 0) {
		AERROR("track timing failed: %d", ret);
		return false;
	}
	ALOG("track timing ok: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		buf[0], buf[1], buf[2], buf[3], buf[4],
		buf[5], buf[6], buf[7], buf[8], buf[9],
		buf[10], buf[11], buf[12], buf[13], buf[14]);
	return true;
}

bool DiskDriveCode::SetReferenceSector(int sector, int trackno)
{
	int ret;

	SIO_parameters params;
	params.device_id = 0x31;
	params.command = eCMDSetRef;
	params.timeout = 7;
	params.data_length = 0;
	params.direction = 0;
	params.data_buffer = 0;
	params.aux1 = sector;
	params.aux2 = trackno;

	ret = fSIOWrapper->DirectSIO(params);
	if (ret != 0) {
		AERROR("setting reference sector failed: %d", ret);
		return false;
	}
	return true;
}

bool DiskDriveCode::ScanTrack(int trackno, uint8_t* buf)
{
	int ret;

	SIO_parameters params;
	params.device_id = 0x31;
	params.command = eCMDScan;
	params.timeout = 20;
	params.data_length = 128;
	params.direction = 0;
	params.data_buffer = buf;
	params.aux1 = trackno;
	params.aux2 = 0;

	ret = fSIOWrapper->DirectSIO(params);
	if (ret != 0) {
		AERROR("scanning track %d failed: %d", trackno, ret);
		return false;
	}
	return true;
}

bool DiskDriveCode::SetupSectorRead(uint8_t* buf)
{
	int ret;
	int adr;

	SIO_parameters params;
	params.device_id = 0x31;
	params.command = 'P';
	params.timeout = 7;
	params.data_length = 128;
	params.direction = 1;
	params.data_buffer = buf;
	adr = fBaseAddress + eSetupReadOffset;
	params.aux1 = adr & 0xff;
	params.aux2 = adr >> 8;

	ret = fSIOWrapper->DirectSIO(params);
	if (ret != 0) {
		AERROR("setting up sector read failed: %d", ret);
		return false;
	}
	return true;
}

bool DiskDriveCode::ReadSector(int idx, uint8_t* buf)
{
	int ret;

	SIO_parameters params;
	params.device_id = 0x31;
	params.command = eCMDRead;
	params.timeout = 7;
	params.data_length = 129;
	params.direction = 0;
	params.data_buffer = buf;
	params.aux1 = idx;
	params.aux2 = 0;

	ret = fSIOWrapper->DirectSIO(params);
	if (ret != 0) {
		AERROR("reading sector %d failed: %d", idx, ret);
		return false;
	}
	return true;
}

