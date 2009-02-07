#ifndef SIOWRAPPER_H
#define SIOWRAPPER_H

/*
   SIOWrapper.h - a simple wrapper around the IOCTLs offered by atarisio

   Copyright (C) 2002-2005 Matthias Reichl <hias@horus.com>

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
#include <sys/ioctl.h>
#include <errno.h>

#include "../driver/atarisio.h"
#include "RefCounted.h"
#include "RCPtr.h"

class SIOWrapper : public RefCounted {
public:
	SIOWrapper(const char* devName = 0);
	virtual ~SIOWrapper();

	/*
	 * configuration stuff
	 */

	int GetKernelDriverVersion();

	int SetCableType_1050_2_PC();
	int SetCableType_APE_Prosystem();
	
	enum ESIOServerCommandLine {
		eCommandLine_RI = 0,
		eCommandLine_DSR = 1,
		eCommandLine_CTS = 2
	};
	// false sets default cable type (command connected to RI),
	// true sets alternative type (command connected to DSR).
	int SetSIOServerMode(ESIOServerCommandLine cmdLine = eCommandLine_RI);

	/*
	 * get status code of last operation
	 */
	int GetLastStatus();

	/*
	 * disk drive methods
	 */
	int ReadSector(unsigned char driveNo, unsigned int sector,
		unsigned char* buf, unsigned int length);

	int WriteSector(unsigned char driveNo, unsigned int sector,
		unsigned char* buf, unsigned int length);

	int WriteAndVerifySector(unsigned char driveNo, unsigned int sector,
		unsigned char* buf, unsigned int length);

	int FormatDisk(unsigned char driveNo, unsigned char* buf, unsigned int length);

	int FormatEnhanced(unsigned char driveNo, unsigned char* buf);
	/* buffer length is fixed to 128 bytes */

	int GetStatus(unsigned char driveNo, unsigned char* buf);
	/* buffer length is fixed to 4 bytes */

	int PercomGet(unsigned char driveNo, unsigned char* buf);
	/* buffer length is fixed to 12 bytes */

	int PercomPut(unsigned char driveNo, unsigned char* buf);
	/* buffer length is fixed to 12 bytes */

	/*
	 * generic SIO method
	 */
	int DirectSIO(SIO_parameters& params);

	/*
	 * SIO server methods
	 */

	int WaitForCommandFrame(int otherReadPollDevice=-1);
	/*
	 * return values:
	 * -1 = timeout
	 *  0 = command frame is waiting
	 *  1 = other device has data
	 *  2 = error in select (or caught signal)
	 */

	int GetCommandFrame(SIO_command_frame& frame);
	int SendCommandACK();
	int SendCommandNAK();
	int SendDataACK();
	int SendDataNAK();
	int SendComplete();
	int SendError();

	int SendDataFrame(unsigned char* buf, unsigned int length);
	int ReceiveDataFrame(unsigned char* buf, unsigned int length);

	int SendRawFrame(unsigned char* buf, unsigned int length);
	int ReceiveRawFrame(unsigned char* buf, unsigned int length);

	int SendCommandACKXF551();
	int SendCompleteXF551();
	int SendDataFrameXF551(unsigned char* buf, unsigned int length);

	int SetBaudrate(unsigned int baudrate);
	int SetHighSpeedBaudrate(unsigned int baudrate);
	int SetAutobaud(unsigned int on);
	int SetHighSpeedPause(unsigned int on);

	int SetTapeBaudrate(unsigned int baudrate);
	int SendTapeBlock(unsigned char* buf, unsigned int length);

	unsigned int GetBaudrate();

	int DebugKernelStatus();

	inline int GetDeviceFD() const;

	int EnableTimestampRecording(unsigned int on);
	int GetTimestamps(SIO_timestamps& timestamps);

private:
	int fDeviceFileNo;
	int fLastResult;
};

inline int SIOWrapper::DirectSIO(SIO_parameters& params)
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

inline int SIOWrapper::GetLastStatus()
{
	return fLastResult;
}

inline int SIOWrapper::GetDeviceFD() const
{
	return fDeviceFileNo;
}
#endif
