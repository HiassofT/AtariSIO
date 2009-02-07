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
#include "AtariDebug.h"
#include "Error.h"

static const char* defaultDeviceName = "/dev/atarisio0";

SIOWrapper::SIOWrapper(const char* devName)
	: fDeviceFileNo(-1), fLastResult(0)
{
	if (!devName) {
		devName = defaultDeviceName;
	}
	fDeviceFileNo = open(devName,O_RDWR);

	if (fDeviceFileNo < 0) {
		throw FileOpenError(devName);
	}
	int version;
	version = GetKernelDriverVersion();
	if (version < 0) {
		throw ErrorObject("cannot determine atarisio kernel driver version!");
	}
	if ( (version >> 8) != (ATARISIO_VERSION >> 8) ||
	     (version & 0xff) < (ATARISIO_VERSION & 0xff) ) {
		throw ErrorObject("atarisio kernel driver version mismatch!");
	}
}

SIOWrapper::~SIOWrapper()
{
	if (fDeviceFileNo >= 0) {
		close(fDeviceFileNo);
		fDeviceFileNo=-1;
		fLastResult=0;
	}
}

int SIOWrapper::GetKernelDriverVersion()
{
	return ioctl(fDeviceFileNo, ATARISIO_IOC_GET_VERSION);
}

int SIOWrapper::SetCableType_1050_2_PC()
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SET_MODE, ATARISIO_MODE_1050_2_PC);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::SetCableType_APE_Prosystem()
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SET_MODE, ATARISIO_MODE_PROSYSTEM);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::SetSIOServerMode(ESIOServerCommandLine cmdLine)
{
	int mode;
	switch (cmdLine) {
	case eCommandLine_RI:
		mode = ATARISIO_MODE_SIOSERVER;
		break;
	case eCommandLine_DSR:
		mode = ATARISIO_MODE_SIOSERVER_DSR;
		break;
	case eCommandLine_CTS:
		mode = ATARISIO_MODE_SIOSERVER_CTS;
		break;
	default:
		return 1;
	}

	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SET_MODE, mode);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::ReadSector(unsigned char driveNo, unsigned int sector, 
		unsigned char* buf, unsigned int length)
{
	if (driveNo<1 || driveNo > 8 || buf==0 || length==0 || sector > 65535) {
		return -1;
	}

	SIO_parameters params;
	params.device_id = 48+driveNo;
	params.command = 0x52;
	params.direction = 0;
	params.timeout = 7;
	params.data_buffer = buf;
	params.data_length = length;
	params.aux1 = sector & 0xff;
	params.aux2 = sector >> 8;
	return DirectSIO(params);
}

int SIOWrapper::WriteSector(unsigned char driveNo, unsigned int sector, 
		unsigned char* buf, unsigned int length)
{
	if (driveNo<1 || driveNo > 8 || buf==0 || length==0 || sector > 65535) {
		return -1;
	}

	SIO_parameters params;
	params.device_id = 48+driveNo;
	params.command = 0x50;
	params.direction = 1;
	params.timeout = 7;
	params.data_buffer = buf;
	params.data_length = length;
	params.aux1 = sector & 0xff;
	params.aux2 = sector >> 8;
	return DirectSIO(params);
}

int SIOWrapper::WriteAndVerifySector(unsigned char driveNo, unsigned int sector, 
		unsigned char* buf, unsigned int length)
{
	if (driveNo<1 || driveNo > 8 || buf==0 || length==0 || sector > 65535) {
		return -1;
	}

	SIO_parameters params;
	params.device_id = 48+driveNo;
	params.command = 0x57;
	params.direction = 1;
	params.timeout = 7;
	params.data_buffer = buf;
	params.data_length = length;
	params.aux1 = sector & 0xff;
	params.aux2 = sector >> 8;
	return DirectSIO(params);
}

int SIOWrapper::FormatDisk(unsigned char driveNo, unsigned char* buf, unsigned int length)
{
	if (driveNo<1 || driveNo > 8 || buf==0) {
		return -1;
	}

	SIO_parameters params;
	params.device_id = 48+driveNo;
	params.command = 0x21;
	params.direction = 0;
	params.timeout = 100;
	params.data_buffer = buf;
	params.data_length = length;
	params.aux1 = 0;
	params.aux2 = 0;
	return DirectSIO(params);
}

int SIOWrapper::FormatEnhanced(unsigned char driveNo, unsigned char* buf)
{
	if (driveNo<1 || driveNo > 8 || buf==0) {
		return -1;
	}

	SIO_parameters params;
	params.device_id = 48+driveNo;
	params.command = 0x22;
	params.direction = 0;
	params.timeout = 100;
	params.data_buffer = buf;
	params.data_length = 128;
	params.aux1 = 0;
	params.aux2 = 0;
	return DirectSIO(params);
}

int SIOWrapper::GetStatus(unsigned char driveNo, unsigned char* buf)
{
	if (driveNo<1 || driveNo > 8 || buf==0) {
		return -1;
	}

	SIO_parameters params;
	params.device_id = 48+driveNo;
	params.command = 0x53;
	params.direction = 0;
	params.timeout = 7;
	params.data_buffer = buf;
	params.data_length = 4;
	params.aux1 = 0;
	params.aux2 = 0;
	return DirectSIO(params);
}

int SIOWrapper::PercomGet(unsigned char driveNo, unsigned char* buf)
{
	if (driveNo<1 || driveNo > 8 || buf==0) {
		return -1;
	}

	SIO_parameters params;
	params.device_id = 48+driveNo;
	params.command = 0x4e;
	params.direction = 0;
	params.timeout = 7;
	params.data_buffer = buf;
	params.data_length = 12;
	params.aux1 = 0;
	params.aux2 = 0;
	return DirectSIO(params);
}

int SIOWrapper::PercomPut(unsigned char driveNo, unsigned char* buf)
{
	if (driveNo<1 || driveNo > 8 || buf==0) {
		return -1;
	}

	SIO_parameters params;
	params.device_id = 48+driveNo;
	params.command = 0x4f;
	params.direction = 1;
	params.timeout = 7;
	params.data_buffer = buf;
	params.data_length = 12;
	params.aux1 = 0;
	params.aux2 = 0;
	return DirectSIO(params);
}

int SIOWrapper::WaitForCommandFrame(int otherReadPollDevice)
{
	fd_set read_set;
	fd_set except_set;
	struct timeval tv;

	int ret;

	if (fDeviceFileNo < 0) {
		return -1;
	} else {
		int maxfd=fDeviceFileNo;

		FD_ZERO(&except_set);
		FD_SET(fDeviceFileNo, &except_set);

		if (otherReadPollDevice>=0) {
			if (otherReadPollDevice > maxfd) {
				maxfd = otherReadPollDevice;
			}
			FD_ZERO(&read_set);
			FD_SET(otherReadPollDevice, &read_set);

			// timeout for checking printer queue
			tv.tv_sec = 15;
			tv.tv_usec = 0;

			ret = select(maxfd+1, &read_set, NULL, &except_set, &tv);
			if (ret == -1) {
				return 2;
			}
			if (ret == 0) {
				return -1;
			}
			if (FD_ISSET(fDeviceFileNo, &except_set)) {
				return 0;
			}
			if (FD_ISSET(otherReadPollDevice, &read_set)) {
				return 1;
			}
			DPRINTF("illegal condition using select!");
			return -2;
		} else {
			ret = select(maxfd+1, NULL, NULL, &except_set, NULL);
			if (ret == -1) {
				return 2;
			}
			if (ret == 0) {
				return -1;
			}
			if (FD_ISSET(fDeviceFileNo, &except_set)) {
				return 0;
			}
			DPRINTF("illegal condition using select!");
			return -1;
		}
	}
}

int SIOWrapper::GetCommandFrame(SIO_command_frame& frame)
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_GET_COMMAND_FRAME, &frame);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::SendCommandACK()
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_COMMAND_ACK);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::SendCommandNAK()
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_COMMAND_NAK);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::SendDataACK()
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_DATA_ACK);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::SendDataNAK()
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_DATA_NAK);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::SendComplete()
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_COMPLETE);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::SendError()
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_ERROR);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::SendDataFrame(unsigned char* buf, unsigned int length)
{
	SIO_data_frame frame;

	frame.data_buffer = buf;
	frame.data_length = length;

	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_DATA_FRAME, &frame);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::ReceiveDataFrame(unsigned char* buf, unsigned int length)
{
	SIO_data_frame frame;

	frame.data_buffer = buf;
	frame.data_length = length;

	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_RECEIVE_DATA_FRAME, &frame);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::SendRawFrame(unsigned char* buf, unsigned int length)
{
	SIO_data_frame frame;

	frame.data_buffer = buf;
	frame.data_length = length;

	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_RAW_FRAME, &frame);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::ReceiveRawFrame(unsigned char* buf, unsigned int length)
{
	SIO_data_frame frame;

	frame.data_buffer = buf;
	frame.data_length = length;

	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_RECEIVE_RAW_FRAME, &frame);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}


int SIOWrapper::SendCommandACKXF551()
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_COMMAND_ACK_XF551);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::SendCompleteXF551()
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_COMPLETE_XF551);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::SendDataFrameXF551(unsigned char* buf, unsigned int length)
{
	SIO_data_frame frame;

	frame.data_buffer = buf;
	frame.data_length = length;

	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_DATA_FRAME_XF551, &frame);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::SetBaudrate(unsigned int baudrate)
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SET_BAUDRATE, baudrate);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::SetHighSpeedBaudrate(unsigned int baudrate)
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SET_HIGHSPEED_BAUDRATE, baudrate);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::SetAutobaud(unsigned int on)
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SET_AUTOBAUD, on);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::SetHighSpeedPause(unsigned int on)
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SET_HIGHSPEEDPAUSE, on);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

unsigned int SIOWrapper::GetBaudrate()
{
	int baudrate;

	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = 0;
		baudrate = ioctl(fDeviceFileNo, ATARISIO_IOC_GET_BAUDRATE);
		if (baudrate == -1) {
			fLastResult = errno;
			return ATARISIO_STANDARD_BAUDRATE;
		} else {
			return baudrate;
		}
	}
	return fLastResult;
}

int SIOWrapper::SetTapeBaudrate(unsigned int baudrate)
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SET_TAPE_BAUDRATE, baudrate);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::SendTapeBlock(unsigned char* buf, unsigned int length)
{
	SIO_data_frame frame;

	frame.data_buffer = buf;
	frame.data_length = length;

	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_TAPE_BLOCK, &frame);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::DebugKernelStatus()
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_PRINT_STATUS);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int SIOWrapper::EnableTimestampRecording(unsigned int on)
{
	fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_ENABLE_TIMESTAMPS, on);
	return fLastResult;
}

int SIOWrapper::GetTimestamps(SIO_timestamps& timestamps)
{
	fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_GET_TIMESTAMPS, &timestamps);
	return fLastResult;
}
