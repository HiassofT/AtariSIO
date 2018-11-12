/*
   KernelSIOWrapper.cpp - a simple wrapper around the IOCTLs offered by atarisio

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

#include "KernelSIOWrapper.h"
#include "AtariDebug.h"
#include "Error.h"

KernelSIOWrapper::KernelSIOWrapper(int fileno)
	: super(fileno)
{
	InitializeBaudrates();
}

KernelSIOWrapper::~KernelSIOWrapper()
{ }

bool KernelSIOWrapper::IsKernelWrapper() const
{
	return true;
}

int KernelSIOWrapper::GetKernelDriverVersion()
{
	if (fDeviceFileNo < 0) {
		return -1;
	} else {
		return ioctl(fDeviceFileNo, ATARISIO_IOC_GET_VERSION);
	};
}

int KernelSIOWrapper::Set1050CableType(E1050CableType type)
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
		return fLastResult;
	}
	switch (type) {
	case e1050_2_PC:
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SET_MODE, ATARISIO_MODE_1050_2_PC);
		break;
	case eApeProsystem:
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SET_MODE, ATARISIO_MODE_PROSYSTEM);
		break;
	default:
		fLastResult = EINVAL;
		return fLastResult;
	}

	if (fLastResult == -1) {
		fLastResult = errno;
	}

	return fLastResult;
}

int KernelSIOWrapper::SetSIOServerMode(ESIOServerCommandLine cmdLine)
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

int KernelSIOWrapper::DirectSIO(SIO_parameters& params)
{
	if (fDeviceFileNo<0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_DO_SIO, &params);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int KernelSIOWrapper::ExtSIO(Ext_SIO_parameters& params)
{
	if (fDeviceFileNo<0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_DO_EXT_SIO, &params);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int KernelSIOWrapper::WaitForCommandFrame(int otherReadPollDevice)
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

int KernelSIOWrapper::GetCommandFrame(SIO_command_frame& frame)
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

int KernelSIOWrapper::SendCommandACK()
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

int KernelSIOWrapper::SendCommandNAK()
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

int KernelSIOWrapper::SendDataACK()
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

int KernelSIOWrapper::SendDataNAK()
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

int KernelSIOWrapper::SendComplete()
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

int KernelSIOWrapper::SendError()
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

int KernelSIOWrapper::SendDataFrame(uint8_t* buf, unsigned int length)
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

int KernelSIOWrapper::ReceiveDataFrame(uint8_t* buf, unsigned int length)
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

int KernelSIOWrapper::SendRawFrame(uint8_t* buf, unsigned int length)
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

int KernelSIOWrapper::ReceiveRawFrame(uint8_t* buf, unsigned int length)
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


int KernelSIOWrapper::SendCommandACKXF551()
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

int KernelSIOWrapper::SendCompleteXF551()
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

int KernelSIOWrapper::SendDataFrameXF551(uint8_t* buf, unsigned int length)
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

int KernelSIOWrapper::SetBaudrate(unsigned int baudrate, bool /* now */)
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

int KernelSIOWrapper::SetStandardBaudrate(unsigned int baudrate)
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_SET_STANDARD_BAUDRATE, baudrate);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int KernelSIOWrapper::SetHighSpeedBaudrate(unsigned int baudrate)
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

int KernelSIOWrapper::SetAutobaud(unsigned int on)
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

int KernelSIOWrapper::SetHighSpeedPause(unsigned int on)
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

int KernelSIOWrapper::SetSioTiming(ESIOTiming timing)
{
	switch (timing) {
	case eRelaxedTiming:
		return SetHighSpeedPause(ATARISIO_HIGHSPEEDPAUSE_BOTH);
	default:
		return SetHighSpeedPause(0);
	}
}

SIOWrapper::ESIOTiming KernelSIOWrapper::GetDefaultSioTiming()
{
	return SIOWrapper::eStrictTiming;
}

int KernelSIOWrapper::GetBaudrate()
{
	int baudrate;

	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = 0;
		baudrate = ioctl(fDeviceFileNo, ATARISIO_IOC_GET_BAUDRATE);
		if (baudrate == -1) {
			fLastResult = errno;
			return fStandardBaudrate;
		} else {
			return baudrate;
		}
	}
	return fLastResult;
}

int KernelSIOWrapper::GetExactBaudrate()
{
	int baudrate;

	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = 0;
		baudrate = ioctl(fDeviceFileNo, ATARISIO_IOC_GET_EXACT_BAUDRATE);
		if (baudrate == -1) {
			fLastResult = errno;
			return fStandardBaudrate;
		} else {
			return baudrate;
		}
	}
	return fLastResult;
}


int KernelSIOWrapper::SetTapeBaudrate(unsigned int baudrate)
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

int KernelSIOWrapper::SendTapeBlock(uint8_t* buf, unsigned int length)
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

int KernelSIOWrapper::StartTapeMode()
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_START_TAPE_MODE);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int KernelSIOWrapper::EndTapeMode()
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_END_TAPE_MODE);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int KernelSIOWrapper::SendRawDataNoWait(uint8_t* buf, unsigned int length)
{
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
}

int KernelSIOWrapper::FlushWriteBuffer()
{
	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_FLUSH_WRITE_BUFFER);
		if (fLastResult == -1) {
			fLastResult = errno;
		}
	}
	return fLastResult;
}

int KernelSIOWrapper::SendFskData(uint16_t* bit_delays, unsigned int num_bits)
{
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
}


int KernelSIOWrapper::DebugKernelStatus()
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

int KernelSIOWrapper::EnableTimestampRecording(unsigned int on)
{
	fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_ENABLE_TIMESTAMPS, on);
	return fLastResult;
}

int KernelSIOWrapper::GetTimestamps(SIO_timestamps& timestamps)
{
	fLastResult = ioctl(fDeviceFileNo, ATARISIO_IOC_GET_TIMESTAMPS, &timestamps);
	return fLastResult;
}

unsigned int KernelSIOWrapper::GetBaudrateForPokeyDivisor(unsigned int pokey_div)
{
	int baudrate;

	if (fDeviceFileNo < 0) {
		fLastResult = ENODEV;
	} else {
		fLastResult = 0;
		baudrate = ioctl(fDeviceFileNo, ATARISIO_IOC_GET_BAUDRATE_FOR_POKEY_DIVISOR, pokey_div);
		if (baudrate == -1) {
			fLastResult = errno;
			return 0;
		} else {
			return baudrate;
		}
	}
	return fLastResult;
}

