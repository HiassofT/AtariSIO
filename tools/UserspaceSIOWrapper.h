#ifndef USERSPACESIOWRAPPER_H
#define USERSPACESIOWRAPPER_H

/*
   UserspaceSIOWrapper.h - implementation of low-level SIO in userspace

   Copyright (C) 2011-2019 Matthias Reichl <hias@horus.com>

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

#include <termios.h>
#include "SIOWrapper.h"
#include "MiscUtils.h"

class UserspaceSIOWrapper : public SIOWrapper {
public:
	UserspaceSIOWrapper(int fileno);
	virtual ~UserspaceSIOWrapper();

	/*
	 * configuration stuff
	 */
	virtual bool IsUserspaceWrapper() const;

	virtual int Set1050CableType(E1050CableType type);

	// false sets default cable type (command connected to RI),
	// true sets alternative type (command connected to DSR).
	virtual int SetSIOServerMode(ESIOServerCommandLine cmdLine = eCommandLine_RI);

	/*
	 * generic SIO method (old)
	 */
	virtual int DirectSIO(SIO_parameters& params);

	/*
	 * extended SIO method (new)
	 */
	virtual int ExtSIO(Ext_SIO_parameters& params);

	/*
	 * SIO server methods
	 */

	virtual int WaitForCommandFrame(int otherReadPollDevice=-1);
	/*
	 * return values:
	 * -1 = timeout
	 *  0 = command frame is waiting
	 *  1 = other device has data
	 *  2 = error in select (or caught signal)
	 */

	virtual int GetCommandFrame(SIO_command_frame& frame);
	virtual int SendCommandACK();
	virtual int SendCommandNAK();
	virtual int SendDataACK();
	virtual int SendDataNAK();
	virtual int SendComplete();
	virtual int SendError();

	virtual int SendDataFrame(uint8_t* buf, unsigned int length);
	virtual int ReceiveDataFrame(uint8_t* buf, unsigned int length);

	virtual int SendRawFrame(uint8_t* buf, unsigned int length);
	virtual int ReceiveRawFrame(uint8_t* buf, unsigned int length);

	virtual int SendCommandACKXF551();
	virtual int SendCompleteXF551();
	virtual int SendDataFrameXF551(uint8_t* buf, unsigned int length);

	virtual int SetBaudrate(unsigned int baudrate, bool now = true);
	virtual int SetStandardBaudrate(unsigned int baudrate);
	virtual int SetHighSpeedBaudrate(unsigned int baudrate);
	virtual int SetAutobaud(unsigned int on);
	virtual int SetHighSpeedPause(unsigned int on);

	virtual int SetSioTiming(ESIOTiming timing);
	virtual ESIOTiming GetDefaultSioTiming();

	virtual int SetTapeBaudrate(unsigned int baudrate);
	virtual int SendTapeBlock(uint8_t* buf, unsigned int length);

	/* new TapeBlock methods */

	virtual int StartTapeMode();
	virtual int EndTapeMode();
	virtual int SendRawDataNoWait(uint8_t* buf, unsigned int length);
	virtual int FlushWriteBuffer();

	virtual int SendFskData(uint16_t* bit_delays, unsigned int num_bits);

	virtual int GetBaudrate();
	virtual int GetExactBaudrate();

	virtual int DebugKernelStatus();

	virtual int EnableTimestampRecording(unsigned int on);
	virtual int GetTimestamps(SIO_timestamps& timestamps);

	virtual unsigned int GetBaudrateForPokeyDivisor(unsigned int pokey_div);

private:
	typedef SIOWrapper super;

	bool InitSerialDevice();
	bool ClearControlLines();

	bool SetCommandLine(bool asserted);

	int SendCommandFrame(Ext_SIO_parameters& params);

	int SetBaudrateIfDifferent(unsigned int baudrate, bool now = true);

	uint8_t CalculateChecksum(uint8_t* buf, unsigned int length);
	bool CmdBufChecksumOK();
	bool BufChecksumOK(unsigned int length);

	MiscUtils::TimestampType TimeForBytes(unsigned int length);
	
	int TransmitBuf(uint8_t* buf, unsigned int length, bool waitTransmit = false);
	int TransmitBuf(unsigned int length, bool waitTransmit = false);
	int TransmitByte(uint8_t byte, bool waitTransmit = false);

	void WaitTransmitComplete(unsigned int bytes = 0);

	int ReceiveBuf(uint8_t* buf, unsigned int length, unsigned int additionalTimeout = 0);
	int ReceiveBuf(unsigned int length, unsigned int additionalTimeout = 0);

	// < 0 means error
	int ReceiveByte(unsigned int additionalTimeout = 0);

	bool NanoSleep(unsigned long nsec);

	inline bool MicroSleep(unsigned long usec)
	{
		return NanoSleep(usec * 1000);
	}

	inline bool MilliSleep(unsigned long usec)
	{
		return NanoSleep(usec * 1000 * 1000);
	}

	void TrySwitchbaud();

	int InternalExtSIO(Ext_SIO_parameters& params);

	bool fHaveCommandLine;
	int fCommandLineMask;
	int fCommandLineLow;
	int fCommandLineHigh;
	unsigned int fTapeBaudrate;
	unsigned int fBaudrate;
	bool fDoAutobaud;
	ESIOTiming fSioTiming;
	unsigned int fTapeOldBaudrate;

	struct termios fOriginalTermios;
	bool fRestoreOriginalTermiosOnExit;

	enum {
		eDelayT0 = 1000,
		eDelayT1 = 850,
		eDelayT2Min = 100,
		eDelayT2Max = 20000,
		eCommandExpire = 10000,
		eDelayT3Min = 1000,
		eDelayT3Max = 1600,
		eDelayT4 = 1000,
		eDelayT4Max = 16000,
		eDelayT5 = 300,
		eDataDelay = 150 // between complete and data frame
	};
	enum {
		eReceiveHeadroom = 50000,
		eSendHeadroom = 50000
	};
		

	enum {
		eCmdLength = 4,
		eCmdRawLength = 5,
		eCmdBufLength = 20,
		eMaxDataLength = 8199,
		eBufLength = 8200
	};
	uint8_t fCmdBuf[eCmdBufLength];
	uint8_t fBuf[eBufLength];

	enum ECommandReceiveState {
		eWaitCommandIdle,
		eWaitCommandAssert,
		eReceiveCommandFrame,
		eWaitCommandDeassert,
		eCommandOK,
		eCommandSoftError,
		eCommandHardError
	} fCommandReceiveState;

	void SetWaitCommandIdleState();
	void SetWaitCommandAssertState();
	void SetReceiveCommandState();
	void SetWaitCommandDeassertState();
	void SetCommandOKState();
	void SetCommandSoftErrorState();
	void SetCommandHardErrorState();

	enum {
		eCommandFrameReceiveTimeout = 15000,
		eNoCommandLineIdleTimeout = 15000,
		eNoCommandLineDeassertDelay = 3000
	};


	static const uint8_t cAckByte = 0x41;
	static const uint8_t cNakByte = 0x4e;
	static const uint8_t cCompleteByte = 0x43;
	static const uint8_t cErrorByte = 0x45;

	int fCommandReceiveCount;
	bool fLastCommandOK;

	MiscUtils::TimestampType fCommandFrameTimestamp;
	MiscUtils::TimestampType fCommandFrameTimeout;

	enum {
		eCommandFrameRetries = 13
	};

	enum {
		eSIORetries = 2
	};

};

#endif
