/*
   SIOWrapper.cpp - a simple wrapper around the IOCTLs offered by atarisio

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
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "SIOWrapper.h"
#include "KernelSIOWrapper.h"
#include "UserspaceSIOWrapper.h"
#include "AtariDebug.h"
#include "Error.h"

#ifdef DEFAULT_DEVICE
#define MY_STR(s) #s
#define MY_STR2(s) MY_STR(s)
static const char* defaultDeviceName = MY_STR2(DEFAULT_DEVICE);
#undef MY_STR
#undef MY_STR2
#else
static const char* defaultDeviceName = "/dev/atarisio0";
#endif

SIOWrapper* SIOWrapper::CreateSIOWrapper(const char* devName)
{
	SIOWrapper* wrapper;
	if (!devName) {
		devName = defaultDeviceName;
	}
	int fileno = open(devName, O_RDWR | O_NOCTTY | O_NDELAY);

	if (fileno < 0) {
		throw FileOpenError(devName);
	}
	int version;
	version = ioctl(fileno, ATARISIO_IOC_GET_VERSION);
	if (version < 0) {
		wrapper = new UserspaceSIOWrapper(fileno);
		ALOG("using userspace driver");
	} else {
		if ( (version >> 8) != (ATARISIO_VERSION >> 8) ||
		     (version & 0xff) < (ATARISIO_VERSION & 0xff) ) {
			throw ErrorObject("atarisio kernel driver version mismatch!");
		} else {
			wrapper = new KernelSIOWrapper(fileno);
		}
	}
	wrapper->SetSioTiming(wrapper->GetDefaultSioTiming());
	return wrapper;
}

SIOWrapper::SIOWrapper(int fileno)
	: fDeviceFileNo(fileno), fLastResult(0)
{ }

SIOWrapper::~SIOWrapper()
{
	if (fDeviceFileNo >= 0) {
		close(fDeviceFileNo);
		fDeviceFileNo=-1;
		fLastResult=0;
	}
}

bool SIOWrapper::IsKernelWrapper() const
{
	return false;
}

bool SIOWrapper::IsUserspaceWrapper() const
{
	return false;
}

int SIOWrapper::ReadSector(uint8_t driveNo, uint16_t sector, 
		uint8_t* buf, unsigned int length,
		uint8_t highspeedMode)
{
	if (driveNo<1 || driveNo > 8 || buf==0 || length==0) {
		return -1;
	}

	Ext_SIO_parameters params;
	params.device = 0x31;
	params.unit = driveNo;
	params.command = 0x52;
	params.direction = ATARISIO_EXTSIO_DIR_RECV;
	params.timeout = 7;
	params.data_buffer = buf;
	params.data_length = length;
	params.aux1 = sector & 0xff;
	params.aux2 = sector >> 8;
	params.highspeed_mode = highspeedMode;
	return ExtSIO(params);
}

int SIOWrapper::WriteSector(uint8_t driveNo, uint16_t sector, 
		uint8_t* buf, unsigned int length,
		uint8_t highspeedMode)
{
	if (driveNo<1 || driveNo > 8 || buf==0 || length==0) {
		return -1;
	}

	Ext_SIO_parameters params;
	params.device = 0x31;
	params.unit = driveNo;
	params.command = 0x50;
	params.direction = ATARISIO_EXTSIO_DIR_SEND;
	params.timeout = 7;
	params.data_buffer = buf;
	params.data_length = length;
	params.aux1 = sector & 0xff;
	params.aux2 = sector >> 8;
	params.highspeed_mode = highspeedMode;
	return ExtSIO(params);
}

int SIOWrapper::WriteAndVerifySector(uint8_t driveNo, uint16_t sector, 
		uint8_t* buf, unsigned int length,
		uint8_t highspeedMode)
{
	if (driveNo<1 || driveNo > 8 || buf==0 || length==0) {
		return -1;
	}

	Ext_SIO_parameters params;
	params.device = 0x31;
	params.unit = driveNo;
	params.command = 0x57;
	params.direction = ATARISIO_EXTSIO_DIR_SEND;
	params.timeout = 7;
	params.data_buffer = buf;
	params.data_length = length;
	params.aux1 = sector & 0xff;
	params.aux2 = sector >> 8;
	params.highspeed_mode = highspeedMode;
	return ExtSIO(params);
}

int SIOWrapper::FormatDisk(uint8_t driveNo, uint8_t* buf, unsigned int length,
		uint8_t highspeedMode)
{
	if (driveNo<1 || driveNo > 8 || buf==0) {
		return -1;
	}

	Ext_SIO_parameters params;
	params.device = 0x31;
	params.unit = driveNo;
	params.command = 0x21;
	params.direction = ATARISIO_EXTSIO_DIR_RECV;
	params.timeout = 100;
	params.data_buffer = buf;
	params.data_length = length;
	params.aux1 = 0;
	params.aux2 = 0;
	params.highspeed_mode = highspeedMode;
	return ExtSIO(params);
}

int SIOWrapper::FormatEnhanced(uint8_t driveNo, uint8_t* buf,
		uint8_t highspeedMode)
{
	if (driveNo<1 || driveNo > 8 || buf==0) {
		return -1;
	}

	Ext_SIO_parameters params;
	params.device = 0x31;
	params.unit = driveNo;
	params.command = 0x22;
	params.direction = ATARISIO_EXTSIO_DIR_RECV;
	params.timeout = 100;
	params.data_buffer = buf;
	params.data_length = 128;
	params.aux1 = 0;
	params.aux2 = 0;
	params.highspeed_mode = highspeedMode;
	return ExtSIO(params);
}

int SIOWrapper::GetStatus(uint8_t driveNo, uint8_t* buf,
		uint8_t highspeedMode)
{
	if (driveNo<1 || driveNo > 8 || buf==0) {
		return -1;
	}

	Ext_SIO_parameters params;
	params.device = 0x31;
	params.unit = driveNo;
	params.command = 0x53;
	params.direction = ATARISIO_EXTSIO_DIR_RECV;
	params.timeout = 7;
	params.data_buffer = buf;
	params.data_length = 4;
	params.aux1 = 0;
	params.aux2 = 0;
	params.highspeed_mode = highspeedMode;
	return ExtSIO(params);
}

int SIOWrapper::PercomGet(uint8_t driveNo, uint8_t* buf,
		uint8_t highspeedMode)
{
	if (driveNo<1 || driveNo > 8 || buf==0) {
		return -1;
	}

	Ext_SIO_parameters params;
	params.device = 0x31;
	params.unit = driveNo;
	params.command = 0x4e;
	params.direction = ATARISIO_EXTSIO_DIR_RECV;
	params.timeout = 7;
	params.data_buffer = buf;
	params.data_length = 12;
	params.aux1 = 0;
	params.aux2 = 0;
	params.highspeed_mode = highspeedMode;
	return ExtSIO(params);
}

int SIOWrapper::PercomPut(uint8_t driveNo, uint8_t* buf,
		uint8_t highspeedMode)
{
	if (driveNo<1 || driveNo > 8 || buf==0) {
		return -1;
	}

	Ext_SIO_parameters params;
	params.device = 0x31;
	params.unit = driveNo;
	params.command = 0x4f;
	params.direction = ATARISIO_EXTSIO_DIR_SEND;
	params.timeout = 7;
	params.data_buffer = buf;
	params.data_length = 12;
	params.aux1 = 0;
	params.aux2 = 0;
	params.highspeed_mode = highspeedMode;
	return ExtSIO(params);
}

int SIOWrapper::ImmediateCommand(uint8_t driveNo, uint8_t command,
		uint8_t aux1, uint8_t aux2,
		uint8_t timeout, uint8_t highspeedMode)
{
	if (driveNo<1 || driveNo > 8) {
		return -1;
	}

	Ext_SIO_parameters params;
	params.device = 0x31;
	params.unit = driveNo;
	params.command = command;
	params.direction = ATARISIO_EXTSIO_DIR_IMMEDIATE;
	params.timeout = timeout;
	params.data_buffer = 0;
	params.data_length = 0;
	params.aux1 = aux1;
	params.aux2 = aux2;
	params.highspeed_mode = highspeedMode;
	return ExtSIO(params);
}

void SIOWrapper::InitializeBaudrates()
{
	fStandardBaudrate = GetBaudrateForPokeyDivisor(ATARISIO_POKEY_DIVISOR_STANDARD);
	if (!fStandardBaudrate) {
		throw DeviceInitError("no baudrate for standard SIO speed");
	}
	fHighspeedBaudrate = GetBaudrateForPokeyDivisor(ATARISIO_POKEY_DIVISOR_3XSIO);
	if (!fHighspeedBaudrate) {
		fHighspeedBaudrate = fStandardBaudrate;
	}
}
