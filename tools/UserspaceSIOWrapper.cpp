/*
   UserspaceSIOWrapper.cpp - a simple wrapper around the IOCTLs offered by atarisio

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

#include "UserspaceSIOWrapper.h"
#include "AtariDebug.h"
#include "Error.h"

#define TODO { fLastResult = ENODEV; return fLastResult; }

UserspaceSIOWrapper::UserspaceSIOWrapper(int fileno)
	: super(fileno)
{ }

UserspaceSIOWrapper::~UserspaceSIOWrapper()
{ }

bool UserspaceSIOWrapper::IsUserspaceWrapper() const
{
	return true;
}


int UserspaceSIOWrapper::SetCableType_1050_2_PC()
{
	TODO

#if 0
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SET_MODE, ATARISIO_MODE_1050_2_PC);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::SetCableType_APE_Prosystem()
{
	TODO
#if 0
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SET_MODE, ATARISIO_MODE_PROSYSTEM);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::SetSIOServerMode(ESIOServerCommandLine cmdLine)
{
	TODO
#if 0
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
#endif
}

int UserspaceSIOWrapper::DirectSIO(SIO_parameters& params)
{
	TODO
#if 0
	if (fDeviceFileNo<0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_DO_SIO, &params);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::ExtSIO(Ext_SIO_parameters& params)
{
	TODO
#if 0
	if (fDeviceFileNo<0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_DO_EXT_SIO, &params);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::WaitForCommandFrame(int otherReadPollDevice)
{
//	TODO
	fd_set read_set;
	//fd_set except_set;
	struct timeval tv;

	int ret;

	if (fDeviceFileNo < 0) {
		return -1;
	} else {
		int maxfd=0;

		if (otherReadPollDevice>=0) {
			if (otherReadPollDevice > maxfd) {
				maxfd = otherReadPollDevice;
			}
			FD_ZERO(&read_set);
			FD_SET(otherReadPollDevice, &read_set);

			// timeout for checking printer queue
			tv.tv_sec = 15;
			tv.tv_usec = 0;

			ret = select(maxfd+1, &read_set, NULL, NULL, &tv);
			if (ret == -1) {
				return 2;
			}
			if (ret == 0) {
				return -1;
			}
			if (FD_ISSET(otherReadPollDevice, &read_set)) {
				return 1;
			}
			DPRINTF("illegal condition using select!");
			return -2;
		} else {
			assert(false);
/*
			ret = select(maxfd+1, NULL, NULL, NULL, NULL);
			if (ret == -1) {
				return 2;
			}
			if (ret == 0) {
				return -1;
			}
*/
			DPRINTF("illegal condition using select!");
			return -1;
		}
	}
}

int UserspaceSIOWrapper::GetCommandFrame(SIO_command_frame& frame)
{
	TODO
#if 0
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_GET_COMMAND_FRAME, &frame);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::SendCommandACK()
{
	TODO
#if 0
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_COMMAND_ACK);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::SendCommandNAK()
{
	TODO
#if 0
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_COMMAND_NAK);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::SendDataACK()
{
	TODO
#if 0
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_DATA_ACK);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::SendDataNAK()
{
	TODO
#if 0
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_DATA_NAK);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::SendComplete()
{
	TODO
#if 0
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_COMPLETE);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::SendError()
{
	TODO
#if 0
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_ERROR);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::SendDataFrame(uint8_t* buf, unsigned int length)
{
	TODO
#if 0
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
#endif
}

int UserspaceSIOWrapper::ReceiveDataFrame(uint8_t* buf, unsigned int length)
{
	TODO
#if 0
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
#endif
}

int UserspaceSIOWrapper::SendRawFrame(uint8_t* buf, unsigned int length)
{
	TODO
#if 0
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
#endif
}

int UserspaceSIOWrapper::ReceiveRawFrame(uint8_t* buf, unsigned int length)
{
	TODO
#if 0
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
#endif
}


int UserspaceSIOWrapper::SendCommandACKXF551()
{
	TODO
#if 0
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_COMMAND_ACK_XF551);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::SendCompleteXF551()
{
	TODO
#if 0
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_COMPLETE_XF551);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::SendDataFrameXF551(uint8_t* buf, unsigned int length)
{
	TODO
#if 0
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
#endif
}

int UserspaceSIOWrapper::SetBaudrate(unsigned int baudrate)
{
	TODO
#if 0
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SET_BAUDRATE, baudrate);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::SetHighSpeedBaudrate(unsigned int baudrate)
{
	TODO
#if 0
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SET_HIGHSPEED_BAUDRATE, baudrate);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::SetAutobaud(unsigned int on)
{
	TODO
#if 0
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SET_AUTOBAUD, on);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::SetHighSpeedPause(unsigned int on)
{
	TODO
#if 0
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SET_HIGHSPEEDPAUSE, on);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::GetBaudrate()
{
	TODO
#if 0
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
#endif
}

int UserspaceSIOWrapper::GetExactBaudrate()
{
	TODO
#if 0
	int baudrate;

	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = 0;
		baudrate = ioctl(fDeviceFileNo, ATARISIO_IOC_GET_EXACT_BAUDRATE);
		if (baudrate == -1) {
			fLastResult = errno;
			return ATARISIO_STANDARD_BAUDRATE;
		} else {
			return baudrate;
		}
	}
	return fLastResult;
#endif
}


int UserspaceSIOWrapper::SetTapeBaudrate(unsigned int baudrate)
{
	TODO
#if 0
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SET_TAPE_BAUDRATE, baudrate);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::SendTapeBlock(uint8_t* buf, unsigned int length)
{
	TODO
#if 0
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
#endif
}

int UserspaceSIOWrapper::StartTapeMode()
{
	TODO
#if 0
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_START_TAPE_MODE);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::EndTapeMode()
{
	TODO
#if 0
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_END_TAPE_MODE);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::SendRawDataNoWait(uint8_t* buf, unsigned int length)
{
	TODO
#if 0
	SIO_data_frame frame;

	frame.data_buffer = buf;
	frame.data_length = length;

	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_RAW_DATA_NOWAIT, &frame);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::FlushWriteBuffer()
{
	TODO
#if 0
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_FLUSH_WRITE_BUFFER);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::SendFskData(uint16_t* bit_delays, unsigned int num_bits)
{
	TODO
#if 0
	FSK_data fsk;

	fsk.num_entries = num_bits;
	fsk.bit_time = bit_delays;

	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SEND_FSK_DATA, &fsk);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}


int UserspaceSIOWrapper::DebugKernelStatus()
{
	TODO
#if 0
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_PRINT_STATUS);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::EnableTimestampRecording(unsigned int on)
{
	TODO
#if 0
	fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_ENABLE_TIMESTAMPS, on);
	return fLastResult;
#endif
}

int UserspaceSIOWrapper::GetTimestamps(SIO_timestamps& timestamps)
{
	TODO
#if 0
	fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_GET_TIMESTAMPS, &timestamps);
	return fLastResult;
#endif
}
