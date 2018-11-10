#ifndef KERNELSIOWRAPPER_H
#define KERNELSIOWRAPPER_H

/*
   KernelSIOWrapper.h - a simple wrapper around the IOCTLs offered by atarisio

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

#include "SIOWrapper.h"

class KernelSIOWrapper : public SIOWrapper {
public:
	KernelSIOWrapper(int fileno);
	virtual ~KernelSIOWrapper();

	/*
	 * configuration stuff
	 */

	virtual bool IsKernelWrapper() const;

	int GetKernelDriverVersion();

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
};

#endif
