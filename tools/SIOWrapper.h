#ifndef SIOWRAPPER_H
#define SIOWRAPPER_H

/*
   SIOWrapper.h - base class for kernel and userspace wrappers

   Copyright (C) 2002-2011 Matthias Reichl <hias@horus.com>

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
#include <stdint.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "../driver/atarisio.h"
#include "RefCounted.h"
#include "RCPtr.h"

class SIOWrapper : public RefCounted {
public:
	static SIOWrapper* CreateSIOWrapper(const char* devicename = 0);

	virtual ~SIOWrapper();

	virtual bool IsKernelWrapper() const;
	virtual bool IsUserspaceWrapper() const;

	/*
	 * configuration stuff
	 */

	enum E1050CableType {
		e1050_2_PC,		// command on RTS
		eApeProsystem,		// command on DTR
		eLotharekSwitchable	// command on RTS, inverted
	};

	virtual int Set1050CableType(E1050CableType type) = 0;

	enum ESIOServerCommandLine {
		eCommandLine_RI = 0,
		eCommandLine_DSR = 1,
		eCommandLine_CTS = 2,
		eCommandLine_None = 3
	};
	// false sets default cable type (command connected to RI),
	// true sets alternative type (command connected to DSR).
	virtual int SetSIOServerMode(ESIOServerCommandLine cmdLine = eCommandLine_RI) = 0;

	/*
	 * get status code of last operation
	 */
	inline int GetLastStatus();
	inline int GetDeviceFD() const;

	/*
	 * disk drive methods
	 */
	int ReadSector(uint8_t driveNo, uint16_t sector,
		uint8_t* buf, unsigned int length,
		uint8_t highspeedMode = ATARISIO_EXTSIO_SPEED_NORMAL);

	int WriteSector(uint8_t driveNo, uint16_t sector,
		uint8_t* buf, unsigned int length,
		uint8_t highspeedMode = ATARISIO_EXTSIO_SPEED_NORMAL);

	int WriteAndVerifySector(uint8_t driveNo, uint16_t sector,
		uint8_t* buf, unsigned int length,
		uint8_t highspeedMode = ATARISIO_EXTSIO_SPEED_NORMAL);

	int FormatDisk(uint8_t driveNo, uint8_t* buf, unsigned int length,
		uint8_t highspeedMode = ATARISIO_EXTSIO_SPEED_NORMAL);

	int FormatEnhanced(uint8_t driveNo, uint8_t* buf,
		uint8_t highspeedMode = ATARISIO_EXTSIO_SPEED_NORMAL);
	/* buffer length is fixed to 128 bytes */

	int GetStatus(uint8_t driveNo, uint8_t* buf,
		uint8_t highspeedMode = ATARISIO_EXTSIO_SPEED_NORMAL);
	/* buffer length is fixed to 4 bytes */

	int PercomGet(uint8_t driveNo, uint8_t* buf,
		uint8_t highspeedMode = ATARISIO_EXTSIO_SPEED_NORMAL);
	/* buffer length is fixed to 12 bytes */

	int PercomPut(uint8_t driveNo, uint8_t* buf,
		uint8_t highspeedMode = ATARISIO_EXTSIO_SPEED_NORMAL);
	/* buffer length is fixed to 12 bytes */

	int ImmediateCommand(uint8_t driveNo, uint8_t command,
		uint8_t aux1, uint8_t aux2,
		uint8_t timeout = 7,
		uint8_t highspeedMode = ATARISIO_EXTSIO_SPEED_NORMAL);

	/*
	 * generic SIO method (old)
	 */
	virtual int DirectSIO(SIO_parameters& params) = 0;

	/*
	 * extended SIO method (new)
	 */
	virtual int ExtSIO(Ext_SIO_parameters& params) = 0;

	/*
	 * SIO server methods
	 */

	virtual int WaitForCommandFrame(int otherReadPollDevice=-1) = 0;
	/*
	 * return values:
	 * -1 = timeout
	 *  0 = command frame is waiting
	 *  1 = other device has data
	 *  2 = error in select (or caught signal)
	 */

	virtual int GetCommandFrame(SIO_command_frame& frame) = 0;
	virtual int SendCommandACK() = 0;
	virtual int SendCommandNAK() = 0;
	virtual int SendDataACK() = 0;
	virtual int SendDataNAK() = 0;
	virtual int SendComplete() = 0;
	virtual int SendError() = 0;

	virtual int SendDataFrame(uint8_t* buf, unsigned int length) = 0;
	virtual int ReceiveDataFrame(uint8_t* buf, unsigned int length) = 0;

	virtual int SendRawFrame(uint8_t* buf, unsigned int length) = 0;
	virtual int ReceiveRawFrame(uint8_t* buf, unsigned int length) = 0;

	virtual int SendCommandACKXF551() = 0;
	virtual int SendCompleteXF551() = 0;
	virtual int SendDataFrameXF551(uint8_t* buf, unsigned int length) = 0;

	virtual int SetBaudrate(unsigned int baudrate, bool now = true) = 0;
	virtual int SetStandardBaudrate(unsigned int baudrate) = 0;
	virtual int SetHighSpeedBaudrate(unsigned int baudrate) = 0;
	virtual int SetAutobaud(unsigned int on) = 0;
	virtual int SetHighSpeedPause(unsigned int on) = 0;

	enum ESIOTiming {
		eStrictTiming,
		eRelaxedTiming
	};
	virtual int SetSioTiming(ESIOTiming timing) = 0;
	virtual ESIOTiming GetDefaultSioTiming() = 0;

	virtual int SetTapeBaudrate(unsigned int baudrate) = 0;
	virtual int SendTapeBlock(uint8_t* buf, unsigned int length) = 0;

	/* new TapeBlock methods */

	virtual int StartTapeMode() = 0;
	virtual int EndTapeMode() = 0;
	virtual int SendRawDataNoWait(uint8_t* buf, unsigned int length) = 0;
	virtual int FlushWriteBuffer() = 0;

	virtual int SendFskData(uint16_t* bit_delays, unsigned int num_bits) = 0;

	virtual int GetBaudrate() = 0;
	virtual int GetExactBaudrate() = 0;

	virtual int DebugKernelStatus() = 0;


	virtual int EnableTimestampRecording(unsigned int on) = 0;
	virtual int GetTimestamps(SIO_timestamps& timestamps) = 0;

	virtual unsigned int GetBaudrateForPokeyDivisor(unsigned int pokey_div) = 0;

	inline unsigned int GetStandardBaudrate() {
		return fStandardBaudrate;
	}

	inline unsigned int GetHighspeedBaudrate() {
		return fHighspeedBaudrate;
	}

protected:
	SIOWrapper(int fileno);

	void InitializeBaudrates();

	int fDeviceFileNo;
	int fLastResult;
	unsigned int fStandardBaudrate;
	unsigned int fHighspeedBaudrate;
};

inline int SIOWrapper::GetLastStatus()
{
	return fLastResult;
}

inline int SIOWrapper::GetDeviceFD() const
{
	return fDeviceFileNo;
}
#endif
