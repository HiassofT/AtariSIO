/*
   UserspaceSIOWrapper.cpp - a simple wrapper around the IOCTLs offered by atarisio

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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/serial.h>

#include "UserspaceSIOWrapper.h"
#include "Termios2.h"
#include "AtariDebug.h"
#include "Error.h"

#define TODO { fLastResult = ENODEV; return fLastResult; }

//#define UTRACE_MASK 0x04
#define UTRACE_MASK 0x00

#define UTRACE_CMD_STATE(x...) \
	if (UTRACE_MASK & 0x01) \
		DPRINTF(x)

#define UTRACE_CMD_DATA(x...) \
	if (UTRACE_MASK & 0x02) \
		DPRINTF(x)

#define UTRACE_CMD_ERROR(x...) \
	if (UTRACE_MASK & 0x04) \
		DPRINTF(x)

#define UTRACE_BAUDRATE(x...) \
	if (UTRACE_MASK & 0x08) \
		DPRINTF(x)

#define UTRACE_1050_2_PC(x...) \
	if (UTRACE_MASK & 0x10) \
		DPRINTF(x)

#define UTRACE_SIO_BEGIN(x...) \
	if (UTRACE_MASK & 0x20) \
		DPRINTF("begin " x)

#define UTRACE_SIO_END(x...) \
	if (UTRACE_MASK & 0x40) \
		DPRINTF("end " x)

#define UTRACE_TRANSMIT(x...) \
	if (UTRACE_MASK & 0x80) \
		DPRINTF(x)

#define UTRACE_WAIT_TRANSMIT(x...) \
	if (UTRACE_MASK & 0x100) \
		DPRINTF(x)

#define UTRACE_WAIT_COMMAND(x...) \
	if (UTRACE_MASK & 0x200) \
		DPRINTF(x)

#define UTRACE_RECEIVE_BUF(x...) \
	if (UTRACE_MASK & 0x400) \
		DPRINTF(x)

#define CMD_BUF_TRACE \
	"[ %02x %02x %02x %02x %02x ]", \
		fCmdBuf[0], fCmdBuf[1], fCmdBuf[2], fCmdBuf[3], fCmdBuf[4]

#define UTRACE_CMD_BUF_OK UTRACE_CMD_DATA(CMD_BUF_TRACE)

#define UTRACE_CMD_BUF_ERROR UTRACE_CMD_ERROR(CMD_BUF_TRACE)

bool UserspaceSIOWrapper::InitSerialDevice()
{
	int flags = fcntl(fDeviceFileNo, F_GETFL, 0);
	if (flags < 0) {
		return false;
	}
	if (fcntl(fDeviceFileNo, F_SETFL, flags | O_NONBLOCK) < 0) {
		AERROR("cannot enable nonblocking mode");
		return false;
	}

	struct serial_struct ss;
        if (ioctl(fDeviceFileNo, TIOCGSERIAL, &ss)) {
                AERROR("get serial info failed");
                return false;
        }

	ss.flags |= ASYNC_LOW_LATENCY;
        if (ioctl(fDeviceFileNo, TIOCSSERIAL, &ss)) {
                AWARN("enabling low latency mode failed");
        }

	struct termios tio;
	if (ioctl(fDeviceFileNo, TCGETS, &tio)) {
		return false;
	}
	// equivalent of cfmakeraw 
	tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	tio.c_oflag &= ~OPOST;
	tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tio.c_cflag &= ~(CSIZE | CSTOPB | PARENB | CBAUD | CBAUD << LINUX_IBSHIFT);
	tio.c_cflag |= HUPCL | CS8 | CLOCAL | CREAD | B19200 | B19200 << LINUX_IBSHIFT;
	tio.c_cc[VMIN] = 0;
	tio.c_cc[VTIME] = 0;
	if (ioctl(fDeviceFileNo, TCSETS, &tio)) {
		AERROR("error configuring serial port");
		return false;
	}

	return true;
}

UserspaceSIOWrapper::UserspaceSIOWrapper(int fileno)
	: super(fileno),
	  fTapeBaudrate(ATARISIO_TAPE_BAUDRATE),
	  fBaudrate(0),
	  fDoAutobaud(false),
	  fSioTiming(SIOWrapper::eRelaxedTiming),
	  fRestoreOriginalTermiosOnExit(true),
	  fLastCommandOK(true)
{
	if (ioctl(fDeviceFileNo, TCGETS, &fOriginalTermios)) {
		fDeviceFileNo = -1;
		throw DeviceInitError("cannot get current serial port settings");
	}
	if (!InitSerialDevice()) {
		throw DeviceInitError("cannot initialize serial port");
	}

	InitializeBaudrates();

	if (SetBaudrate(fStandardBaudrate)) {
		throw DeviceInitError("cannot set standard baudrate");
	}
	tcflush(fDeviceFileNo, TCIOFLUSH);
}

UserspaceSIOWrapper::~UserspaceSIOWrapper()
{
	if (fDeviceFileNo >= 0 && fRestoreOriginalTermiosOnExit) {
		ioctl(fDeviceFileNo, TCSETS, &fOriginalTermios);
	}

	struct serial_struct ss;
        if (!ioctl(fDeviceFileNo, TIOCGSERIAL, &ss)) {
            	ss.flags &= ~ASYNC_LOW_LATENCY;
	        ioctl(fDeviceFileNo, TIOCSSERIAL, &ss);
        }
}

bool UserspaceSIOWrapper::IsUserspaceWrapper() const
{
	return true;
}

uint8_t UserspaceSIOWrapper::CalculateChecksum(uint8_t* buf, unsigned int length)
{
	uint16_t cksum = 0;
	for (unsigned int i = 0; i < length; i++) {
		cksum += buf[i];
		if (cksum >= 0x100) {
			cksum = (cksum & 0xff) + 1;
		}
	}
	return (uint8_t) cksum;
}

bool UserspaceSIOWrapper::CmdBufChecksumOK()
{
	return fCmdBuf[eCmdLength] == CalculateChecksum(fCmdBuf, eCmdLength);
}

bool UserspaceSIOWrapper::BufChecksumOK(unsigned int length)
{
	return fBuf[length] == CalculateChecksum(fBuf, length);
}

bool UserspaceSIOWrapper::SetCommandLine(bool asserted)
{
	int flags;
	if (ioctl(fDeviceFileNo, TIOCMGET, &flags)) {
		return false;
	}

	flags &= fCommandLineMask;

	if (asserted) {
		flags |= fCommandLineLow;
	} else {
		flags |= fCommandLineHigh;
	}

	if (ioctl(fDeviceFileNo, TIOCMSET, &flags)) {
		return false;
	}
	return true;
}


int UserspaceSIOWrapper::Set1050CableType(E1050CableType type)
{
	fCommandLineMask = ~(TIOCM_RTS | TIOCM_DTR);

	fLastResult = 0;

	switch (type) {
	case e1050_2_PC:
		fCommandLineLow  = TIOCM_RTS | TIOCM_DTR;
		fCommandLineHigh =             TIOCM_DTR;
		break;
	case eApeProsystem:
		fCommandLineLow  = TIOCM_RTS | TIOCM_DTR;
		fCommandLineHigh = TIOCM_RTS;
		break;
	case eLotharekSwitchable:
		fCommandLineLow  = 0;
		fCommandLineHigh = TIOCM_RTS;

		// workaround for keeping command line deasserted after exit
		fRestoreOriginalTermiosOnExit = false;

		struct termios tio;
		if (ioctl(fDeviceFileNo, TCGETS, &tio)) {
			fLastResult = errno;
			return fLastResult;
		}
		// don't clear control lines on close
		tio.c_cflag &= ~(HUPCL);
		if (ioctl(fDeviceFileNo, TCSETS, &tio)) {
			fLastResult = errno;
			return fLastResult;
		}

		break;
	default:
		fLastResult = EINVAL;
		return fLastResult;
	}

	if (SetCommandLine(false)) {
		fLastResult = 0;
	} else {
		fLastResult = 1;
	}

	SetBaudrate(fStandardBaudrate);

	// wait 0.5 sec for voltage supply to stabilize
	MilliSleep(500);

	tcflush(fDeviceFileNo, TCIOFLUSH);

	return fLastResult;
}

bool UserspaceSIOWrapper::ClearControlLines()
{
	int flags;
	if (ioctl(fDeviceFileNo, TIOCMGET, &flags)) {
		return false;
	}

	// clear DTR and RTS to make autoswitching Atarimax interface work
	flags &= ~(TIOCM_DTR | TIOCM_RTS);

	if (ioctl(fDeviceFileNo, TIOCMSET, &flags)) {
		return false;
	}
	return true;
}

int UserspaceSIOWrapper::SetSIOServerMode(ESIOServerCommandLine cmdLine)
{
	fLastResult = 0;
	fHaveCommandLine = true;
	switch (cmdLine) {
	case eCommandLine_RI:
		fCommandLineMask = TIOCM_RI;
		break;
	case eCommandLine_DSR:
		fCommandLineMask = TIOCM_DSR;
		break;
	case eCommandLine_CTS:
		fCommandLineMask = TIOCM_CTS;
		break;
	case eCommandLine_None:
		fHaveCommandLine = false;
		break;
	default:
		fHaveCommandLine = false;
		fLastResult = 1;
	}

	// clear DTR and RTS to make autoswitching Atarimax interface work
	ClearControlLines();
	SetWaitCommandIdleState();

	return fLastResult;
}

int UserspaceSIOWrapper::DirectSIO(SIO_parameters& /* params */)
{
	TODO
}

int UserspaceSIOWrapper::ExtSIO(Ext_SIO_parameters& params)
{
	if (params.data_length > eMaxDataLength) {
		fLastResult = EATARISIO_ERROR_BLOCK_TOO_LONG;
		return fLastResult;
	}
	switch (params.highspeed_mode) {
	case ATARISIO_EXTSIO_SPEED_NORMAL:
	case ATARISIO_EXTSIO_SPEED_ULTRA:
		break;
	default:
		fLastResult = EATARISIO_UNKNOWN_ERROR;
		break;
	};

	fLastResult = InternalExtSIO(params);
	return fLastResult;
}

void UserspaceSIOWrapper::SetWaitCommandIdleState()
{
	UTRACE_CMD_STATE("State WaitCommandIdle, cmd = %d", fHaveCommandLine);
	fCommandReceiveState = eWaitCommandIdle;
	if (!fHaveCommandLine) {
		// wait for 15msec idle before trying to fetch a command rame
		fCommandFrameTimeout = MiscUtils::GetCurrentTimePlusUsec(eNoCommandLineIdleTimeout);
	}
}

void UserspaceSIOWrapper::SetWaitCommandAssertState()
{
	UTRACE_CMD_STATE("State WaitCommandAssert, cmd = %d", fHaveCommandLine);
	fCommandReceiveState = eWaitCommandAssert;
}

void UserspaceSIOWrapper::SetReceiveCommandState()
{
	UTRACE_CMD_STATE("State ReceiveCommandFrame, cmd = %d", fHaveCommandLine);
	fCommandReceiveState = eReceiveCommandFrame;
	fCommandReceiveCount = 0;
	fCommandFrameTimeout = MiscUtils::GetCurrentTimePlusUsec(eCommandFrameReceiveTimeout);
}

void UserspaceSIOWrapper::SetWaitCommandDeassertState()
{
	UTRACE_CMD_STATE("State WaitCommandDeassert, cmd = %d", fHaveCommandLine);
	if (!fHaveCommandLine) {
		fCommandFrameTimeout = MiscUtils::GetCurrentTimePlusUsec(eNoCommandLineDeassertDelay);
	}
	fCommandReceiveState = eWaitCommandDeassert;
}

void UserspaceSIOWrapper::SetCommandOKState()
{
	UTRACE_CMD_STATE("State CommandOK, cmd = %d", fHaveCommandLine);
	fCommandFrameTimestamp = MiscUtils::GetCurrentTime();
	fCommandReceiveState = eCommandOK;
	fLastCommandOK = true;
}

void UserspaceSIOWrapper::SetCommandSoftErrorState()
{
	UTRACE_CMD_STATE("State CommandSoftError, cmd = %d", fHaveCommandLine);
	fCommandReceiveState = eCommandSoftError;
}

void UserspaceSIOWrapper::SetCommandHardErrorState()
{
	UTRACE_CMD_STATE("State CommandHardError, cmd = %d", fHaveCommandLine);
	fCommandReceiveState = eCommandHardError;
}

int UserspaceSIOWrapper::WaitForCommandFrame(int otherReadPollDevice)
{
	fd_set read_set;
	struct timeval tv;
	int maxfd;
	int flags;
	int sel;
	int cnt;
	MiscUtils::TimestampType printerTimeout = MiscUtils::GetCurrentTimePlusSec(15);
	MiscUtils::TimestampType now;

	while (true) {
		now = MiscUtils::GetCurrentTime();

		tv.tv_sec = 0;
		tv.tv_usec = 100;

		FD_ZERO(&read_set);
		FD_SET(fDeviceFileNo, &read_set);
			maxfd = fDeviceFileNo;

		if (otherReadPollDevice >= 0) {
			if (otherReadPollDevice > maxfd) {
				maxfd = otherReadPollDevice;
			}
			FD_SET(otherReadPollDevice, &read_set);
		}

		switch (fCommandReceiveState) {
		case eCommandSoftError:
			if (fLastCommandOK) {
				fLastCommandOK = false;
				SetWaitCommandIdleState();
				printerTimeout = now + MiscUtils::SecToTimestamp(15);
				continue;
			}
			// fallthrough

		case eCommandHardError:
			fLastCommandOK = false;
			SetWaitCommandIdleState();
			printerTimeout = now + MiscUtils::SecToTimestamp(15);
			TrySwitchbaud();
			continue;
			
		case eWaitCommandIdle:
			if (fHaveCommandLine) {
				if (ioctl(fDeviceFileNo, TIOCMGET, &flags)) {
					AERROR("failed to read modem line status");
					break;
				}
				if (!(flags & fCommandLineMask)) {
					SetWaitCommandAssertState();
					continue;
				}
			} else {
				if (now > fCommandFrameTimeout) {
					SetWaitCommandAssertState();
					continue;
				}
			}
			break;

		case eWaitCommandAssert:
			if (fHaveCommandLine) {
				if (ioctl(fDeviceFileNo, TIOCMGET, &flags)) {
					AERROR("failed to read modem line status");
					break;
				}

				if (flags & fCommandLineMask) {
					SetReceiveCommandState();
					continue;
				} else {
					tcflush(fDeviceFileNo, TCIFLUSH);
					UTRACE_WAIT_COMMAND("WaitCommandAssert: flushed input");
				}
			}
			break;

		case eReceiveCommandFrame:
			if (fCommandReceiveCount == eCmdRawLength) {
				if (CmdBufChecksumOK()) {
					UTRACE_CMD_BUF_OK;
					SetWaitCommandDeassertState();
					continue;
				} else {
					UTRACE_CMD_ERROR("command frame checksum error");
					UTRACE_CMD_BUF_ERROR;
					SetCommandSoftErrorState();
					continue;
				}
			}

			if (fCommandReceiveCount > eCmdRawLength) {
				UTRACE_CMD_ERROR("too many command frame data bytes (%d)", fCommandReceiveCount);
				SetCommandHardErrorState();
				continue;
			}

			if (now > fCommandFrameTimeout) {
				UTRACE_CMD_ERROR("receive timeout expired, got %d bytes", fCommandReceiveCount);
				UTRACE_CMD_BUF_ERROR;
				if (fCommandReceiveCount < 4) {
					SetCommandHardErrorState();
				} else {
					SetCommandSoftErrorState();
				}
				continue;
			}

			tv.tv_usec = 1000;
			break;
		case eWaitCommandDeassert:
			if (fHaveCommandLine) {
				if (ioctl(fDeviceFileNo, TIOCMGET, &flags)) {
					AERROR("failed to read modem line status");
					break;
				}
				if (!(flags & fCommandLineMask)) {
					SetCommandOKState();
					return 0;
				}
			} else {
				if (now > fCommandFrameTimeout) {
					SetCommandOKState();
					return 0;
				}
			}
			break;
		case eCommandOK:
			SetWaitCommandIdleState();
			continue;
		}

		sel = select(maxfd + 1, &read_set, NULL, NULL, &tv);

		if (sel < 0) {
			SetCommandHardErrorState();
			return 2;
		}

		if (sel > 0) {
			if (FD_ISSET(fDeviceFileNo, &read_set)) {
				switch (fCommandReceiveState) {
				case eReceiveCommandFrame:
					cnt = read(fDeviceFileNo, fCmdBuf + fCommandReceiveCount, 10);
					if (cnt < 0) {
						AERROR("failed to read command frame data");
						SetCommandHardErrorState();
						continue;
					}
					fCommandReceiveCount += cnt;
					UTRACE_CMD_DATA("got %d command frame bytes (%d total)",
						cnt, fCommandReceiveCount
					);
					break;
				case eWaitCommandIdle:
					tcflush(fDeviceFileNo, TCIFLUSH);
					if (!fHaveCommandLine) {
						fCommandFrameTimeout = MiscUtils::GetCurrentTimePlusUsec(eNoCommandLineIdleTimeout);
					}
					break;
				case eWaitCommandAssert:
					// command line case is handled in switch before
					if (!fHaveCommandLine) {
						SetReceiveCommandState();
					}
					break;
				case eWaitCommandDeassert:
					UTRACE_CMD_ERROR("data received in WaitCommandDeassert");
					SetCommandHardErrorState();
					break;
				default:
					AERROR("unexpected input in state %d, flushing it", fCommandReceiveState);
					SetCommandHardErrorState();
					break;
				}
			}
			if (otherReadPollDevice >= 0 && FD_ISSET(otherReadPollDevice, &read_set)) {
				return 1;
			}
		}

		if (printerTimeout && now > printerTimeout) {
			return -1;
		}
	}
}

int UserspaceSIOWrapper::GetCommandFrame(SIO_command_frame& frame)
{
	if (fCommandReceiveState == eCommandOK) {
		frame.device_id = fCmdBuf[0];
		frame.command = fCmdBuf[1];
		frame.aux1 = fCmdBuf[2];
		frame.aux2 = fCmdBuf[3];
		frame.reception_timestamp = fCommandFrameTimestamp;
		frame.missed_count = 0;
		SetWaitCommandAssertState();
		return 0;
	} else {
		return ENOMSG;
	}
}

MiscUtils::TimestampType UserspaceSIOWrapper::TimeForBytes(unsigned int length)
{
	return ((MiscUtils::TimestampType) length * 10 * 1000000) / fBaudrate;
}

bool UserspaceSIOWrapper::NanoSleep(unsigned long nsec) {
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = nsec;
	return nanosleep(&ts, NULL);
}

void UserspaceSIOWrapper::WaitTransmitComplete(unsigned int bytes)
{
	int cnt;
	unsigned int lsr;

	if (!bytes) {
		// optimize for speed switch after get sio byte
		bytes = 2;
	}
	UTRACE_WAIT_TRANSMIT("begin WaitTransmitComplete()");

	// tcdrain can block for 2-3 additional jiffies if there's data
	// to transmit, so wait a short time before calling it.

	MiscUtils::TimestampType delay;
	switch (fSioTiming) {
	case eStrictTiming:
		delay = 100 + TimeForBytes(bytes) * 105 / 100;
		break;
	default:
		delay = 200 + TimeForBytes(bytes) * 15/10;
		if (delay < 500) {
			delay = 500;
		}
		break;
	}

	if (delay > 5000) {
		delay = 5000;
	}

	MicroSleep(delay);

	UTRACE_WAIT_TRANSMIT("tcdrain start");
	tcdrain(fDeviceFileNo);
	UTRACE_WAIT_TRANSMIT("tcdrain finished");

	// tcdrain should handle that, but better check it
	cnt = 0;
	while (cnt++ < 10) {
		if (ioctl(fDeviceFileNo, TIOCSERGETLSR, &lsr)) {
			break;
		}
		if (lsr & TIOCSER_TEMT) {
			break;
		}
		NanoSleep(100000);
	}
	UTRACE_WAIT_TRANSMIT("checked lsr %d times", cnt);

	switch (fSioTiming) {
	case eStrictTiming:
		break;
	default:
		// one byte could still be in the transmitter holding register
		NanoSleep(TimeForBytes(1) * 1500);
	}
	UTRACE_WAIT_TRANSMIT("end WaitTransmitComplete");
}

int UserspaceSIOWrapper::TransmitBuf(uint8_t* buf, unsigned int length, bool waitTransmit)
{
	// timeout with 20% margin
	MiscUtils::TimestampType to = TimeForBytes(length) * 12 / 10 + eDelayT3Max + eSendHeadroom;

	unsigned int pos = 0;
	int cnt;
	int sel;

	fd_set write_set;
	struct timeval tv;

	MiscUtils::TimestampType now = MiscUtils::GetCurrentTime();
	MiscUtils::TimestampType endTime = now + to;

	UTRACE_TRANSMIT("begin TransmitBuf %d", length);

	while (pos < length) {
		FD_ZERO(&write_set);
		FD_SET(fDeviceFileNo, &write_set);
		now = MiscUtils::GetCurrentTime();
		if (now >= endTime) {
			// DPRINTF("timeout in TransmitBuf(%d)", length);
			return EATARISIO_COMMAND_TIMEOUT;
		}
		MiscUtils::TimestampToTimeval(endTime - now, tv);
		sel = select(fDeviceFileNo + 1, NULL, &write_set, NULL, &tv);
		if (sel < 0) {
			// DPRINTF("select failed in TransmitBuf(%d): ", length, errno);
			return EATARISIO_UNKNOWN_ERROR;
		}
		if (sel == 1) {
			cnt = write(fDeviceFileNo, buf + pos, length - pos);
			if (cnt < 0) {
				// DPRINTF("write failed in TransmitBuf(%d): ", length, errno);
				return EATARISIO_UNKNOWN_ERROR;
			}
			pos += cnt;
		}
	}

	if (waitTransmit) {
		UTRACE_TRANSMIT("WaitTransmitComplete");
		WaitTransmitComplete(length);
	}

	UTRACE_TRANSMIT("end TransmitBuf %d", length);
	return 0;
}

int UserspaceSIOWrapper::TransmitBuf(unsigned int length, bool waitTransmit)
{
	return TransmitBuf(fBuf, length, waitTransmit);
}


int UserspaceSIOWrapper::TransmitByte(uint8_t byte, bool waitTransmit)
{
	int ret = TransmitBuf(&byte, 1, waitTransmit);
	return ret;
}

int UserspaceSIOWrapper::ReceiveBuf(uint8_t* buf, unsigned int length, unsigned int additionalTimeout)
{
	// timeout with 20% margin
	MiscUtils::TimestampType to = TimeForBytes(length) * 12 / 10 + additionalTimeout;

	unsigned int pos = 0;
	int cnt;
	int sel;
	
	fd_set read_set;
	struct timeval tv;

	MiscUtils::TimestampType now = MiscUtils::GetCurrentTime();
	MiscUtils::TimestampType endTime = now + to;

	while (pos < length) {
		FD_ZERO(&read_set);
		FD_SET(fDeviceFileNo, &read_set);
		now = MiscUtils::GetCurrentTime();
		if (now >= endTime) {
			UTRACE_RECEIVE_BUF("receive timeout, got %d of %d bytes",
				pos, length);
			return EATARISIO_COMMAND_TIMEOUT;
		}
		MiscUtils::TimestampToTimeval(endTime - now, tv);
		sel = select(fDeviceFileNo + 1, &read_set, NULL, NULL, &tv);
		if (sel < 0) {
			return EATARISIO_UNKNOWN_ERROR;
		}
		if (sel == 1) {
			cnt = read(fDeviceFileNo, buf + pos, length - pos);
			if (cnt < 0) {
				return EATARISIO_UNKNOWN_ERROR;
			}
			pos += cnt;
		}
	}
	return 0;
}

int UserspaceSIOWrapper::ReceiveBuf(unsigned int length, unsigned int additionalTimeout)
{
	return ReceiveBuf(fBuf, length, additionalTimeout);
}

int UserspaceSIOWrapper::ReceiveByte(unsigned int additionalTimeout)
{
	uint8_t byte;
	int ret;
	ret = ReceiveBuf(&byte, 1, additionalTimeout);
	if (ret) {
		return -ret;
	} else {
		return byte;
	}
}

int UserspaceSIOWrapper::SendCommandACK()
{
	MiscUtils::TimestampType now = MiscUtils::GetCurrentTime();
	if (now >= fCommandFrameTimestamp + eCommandExpire) {
		UTRACE_CMD_ERROR("SendCommandACK: command frame is too old (%lu usec)",
			(unsigned long) (now - fCommandFrameTimestamp));
		fLastResult = EATARISIO_COMMAND_TIMEOUT;
	} else {
		UTRACE_SIO_BEGIN("SendCommandACK");
		MicroSleep(eDelayT2Min);
		fLastResult = TransmitByte(cAckByte, true);
		UTRACE_SIO_END("SendCommandACK");
	}
	return fLastResult;
}

int UserspaceSIOWrapper::SendCommandNAK()
{
	MiscUtils::TimestampType now = MiscUtils::GetCurrentTime();
	if (now >= fCommandFrameTimestamp + eCommandExpire) {
		UTRACE_CMD_ERROR("SendCommandNAK: command frame is too old (%lu usec)",
			(unsigned long) (now - fCommandFrameTimestamp));
		fLastResult = EATARISIO_COMMAND_TIMEOUT;
	} else {
		UTRACE_SIO_BEGIN("SendCommandNAK");
		MicroSleep(eDelayT2Min);
		fLastResult = TransmitByte(cNakByte);
		UTRACE_SIO_END("SendCommandNAK");
	}
	return fLastResult;
}

int UserspaceSIOWrapper::SendDataACK()
{
	UTRACE_SIO_BEGIN("SendDataACK");
	MicroSleep(eDelayT4);
	fLastResult = TransmitByte(cAckByte, true);
	UTRACE_SIO_END("SendDataACK");
	return fLastResult;
}

int UserspaceSIOWrapper::SendDataNAK()
{
	UTRACE_SIO_BEGIN("SendDataNAK");
	MicroSleep(eDelayT4);
	fLastResult = TransmitByte(cNakByte);
	UTRACE_SIO_END("SendDataNAK");
	return fLastResult;
}

int UserspaceSIOWrapper::SendComplete()
{
	UTRACE_SIO_BEGIN("SendComplete");
	MicroSleep(eDelayT5);
	fLastResult = TransmitByte(cCompleteByte);
	UTRACE_SIO_END("SendComplete");
	return fLastResult;
}

int UserspaceSIOWrapper::SendError()
{
	UTRACE_SIO_BEGIN("SendError");
	MicroSleep(eDelayT5);
	fLastResult = TransmitByte(cErrorByte);
	UTRACE_SIO_END("SendError");
	return fLastResult;
}

int UserspaceSIOWrapper::SendDataFrame(uint8_t* buf, unsigned int length)
{
	if (length > eMaxDataLength) {
		fLastResult = EATARISIO_ERROR_BLOCK_TOO_LONG;
		return fLastResult;
	}
	UTRACE_SIO_BEGIN("SendDataFrame");
	memcpy(fBuf, buf, length);
	fBuf[length] = CalculateChecksum(fBuf, length);

	// wait for complete to be transmitted
	WaitTransmitComplete(1);

	MicroSleep(eDataDelay);
	fLastResult = TransmitBuf(fBuf, length+1);
	UTRACE_SIO_END("SendDataFrame");
	return fLastResult;
}

int UserspaceSIOWrapper::ReceiveDataFrame(uint8_t* buf, unsigned int length)
{
	UTRACE_SIO_BEGIN("ReceiveDataFrame");
	fLastResult = ReceiveBuf(length+1, eDelayT3Max + eReceiveHeadroom);
	UTRACE_SIO_END("ReceiveDataFrame");
	// DPRINTF("ReceiveBuf(%d): %d", length+1, fLastResult);

	if (fLastResult) {
		return fLastResult;
	}
	if (BufChecksumOK(length)) {
		memcpy(buf, fBuf, length);
		fLastResult = SendDataACK();
	} else {
		SendDataNAK();
		fLastResult = EATARISIO_CHECKSUM_ERROR;
	}
	return fLastResult;
}

int UserspaceSIOWrapper::SendRawFrame(uint8_t* buf, unsigned int length)
{
	fLastResult = TransmitBuf(buf, length);
	return fLastResult;
}

int UserspaceSIOWrapper::ReceiveRawFrame(uint8_t* buf, unsigned int length)
{
	fLastResult = ReceiveBuf(buf, length, eDelayT3Max + eReceiveHeadroom);
	return fLastResult;
}

int UserspaceSIOWrapper::SendCommandACKXF551()
{
	TODO
}

int UserspaceSIOWrapper::SendCompleteXF551()
{
	TODO
}

int UserspaceSIOWrapper::SendDataFrameXF551(uint8_t* /* buf */, unsigned int /* length */)
{
	TODO
}

int UserspaceSIOWrapper::SetBaudrate(unsigned int baudrate, bool now)
{
	bool standardSpeed = true;
	speed_t speed;
	
	UTRACE_BAUDRATE("change baudrate from %d to %d, now = %d", fBaudrate, baudrate, now);

	if (!now) {
		WaitTransmitComplete();
	}

	switch (baudrate) {
	case 600:
		speed = B600;
		break;
	case 1200:
		speed = B1200;
		break;
	case 2400:
		speed = B2400;
		break;
	case 4800:
		speed = B4800;
		break;
	case 9600:
		speed = B9600;
		break;
	case 19200:
		speed = B19200;
		break;
	case 38400:
		speed = B38400;
		break;
	case 57600:
		speed = B57600;
		break;
	default:
		standardSpeed = false;
		break;
	}

	if (standardSpeed) {
		struct termios tios;

		fLastResult = ioctl(fDeviceFileNo, TCGETS, &tios);
		if (fLastResult < 0) {
			return fLastResult;
		}
		cfsetispeed(&tios, speed);
		cfsetospeed(&tios, speed);

		fLastResult = ioctl(fDeviceFileNo, now ? TCSETS : TCSETSW, &tios);
	} else {
		struct termios2 tios2;

		fLastResult = ioctl(fDeviceFileNo, TCGETS2, &tios2);
		if (fLastResult < 0) {
			return fLastResult;
		}
		tios2.c_cflag &= ~(CBAUD | CBAUD << LINUX_IBSHIFT);
		tios2.c_cflag |= BOTHER | BOTHER << LINUX_IBSHIFT;
		tios2.c_ispeed = baudrate;
		tios2.c_ospeed = baudrate;
		fLastResult = ioctl(fDeviceFileNo, now ? TCSETS2 : TCSETSW2, &tios2);
	}

	if (!fLastResult) {
		fBaudrate = baudrate;
	}
	return fLastResult;
}

int UserspaceSIOWrapper::SetBaudrateIfDifferent(unsigned int baudrate, bool now)
{
	if (fBaudrate == baudrate) {
		return 0;
	}
	return SetBaudrate(baudrate, now);
}

void UserspaceSIOWrapper::TrySwitchbaud()
{
	if (fDoAutobaud) {
		if (fBaudrate == fStandardBaudrate) {
			SetBaudrate(fHighspeedBaudrate, true);
		} else {
			SetBaudrate(fStandardBaudrate, true);
		}
	}
	tcflush(fDeviceFileNo, TCIOFLUSH);
}

int UserspaceSIOWrapper::SetStandardBaudrate(unsigned int baudrate)
{
	fStandardBaudrate = baudrate;
	fLastResult = 0;
	return fLastResult;
}

int UserspaceSIOWrapper::SetHighSpeedBaudrate(unsigned int baudrate)
{
	fHighspeedBaudrate = baudrate;
	fLastResult = 0;
	return fLastResult;
}

int UserspaceSIOWrapper::SetAutobaud(unsigned int on)
{
	fDoAutobaud = on;
	fLastResult = 0;
	return fLastResult;
}

int UserspaceSIOWrapper::SetHighSpeedPause(unsigned int on)
{
	if (on) {
		fLastResult = EINVAL;
	} else {
		fLastResult = 0;
	}
	return fLastResult;
}

int UserspaceSIOWrapper::SetSioTiming(ESIOTiming timing)
{
	fSioTiming = timing;
	return 0;
}

SIOWrapper::ESIOTiming UserspaceSIOWrapper::GetDefaultSioTiming()
{
	return SIOWrapper::eRelaxedTiming;
}

int UserspaceSIOWrapper::GetBaudrate()
{
	return fBaudrate;
}

int UserspaceSIOWrapper::GetExactBaudrate()
{
	// TODO
	return fBaudrate;
}


int UserspaceSIOWrapper::SetTapeBaudrate(unsigned int baudrate)
{
	fTapeBaudrate = baudrate;
	if (fBaudrate != baudrate) {
		fLastResult = SetBaudrate(baudrate);
	} else {
		fLastResult = 0;
	}
	return fLastResult;
}

int UserspaceSIOWrapper::SendTapeBlock(uint8_t* /* buf */, unsigned int /* length */)
{
	TODO
}

int UserspaceSIOWrapper::StartTapeMode()
{
	fTapeOldBaudrate = fBaudrate;
	SetBaudrate(fTapeBaudrate);
	tcflush(fDeviceFileNo, TCIOFLUSH);
	fLastResult = 0;
	return fLastResult;
}

int UserspaceSIOWrapper::EndTapeMode()
{
	SetBaudrate(fTapeOldBaudrate, false);
	tcflush(fDeviceFileNo, TCIOFLUSH);
	fLastResult = 0;
	return fLastResult;
}

int UserspaceSIOWrapper::SendRawDataNoWait(uint8_t* buf, unsigned int length)
{
	fLastResult = TransmitBuf(buf, length);
	return fLastResult;
}

int UserspaceSIOWrapper::FlushWriteBuffer()
{
	WaitTransmitComplete();
	fLastResult = 0;
	return fLastResult;
}

int UserspaceSIOWrapper::SendFskData(uint16_t* /* bit_delays */, unsigned int /* num_bits */)
{
	TODO
}


int UserspaceSIOWrapper::DebugKernelStatus()
{
	// NOP
	return 0;
}

int UserspaceSIOWrapper::EnableTimestampRecording(unsigned int /* on */)
{
	// NOP
	return 1;
}

int UserspaceSIOWrapper::GetTimestamps(SIO_timestamps& /* timestamps */)
{
	TODO
}

unsigned int UserspaceSIOWrapper::GetBaudrateForPokeyDivisor(unsigned int divisor)
{
	switch (divisor) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
		// compensate for late pokey sampling, add 3 cycles to byte time
		return (ATARISIO_ATARI_FREQUENCY_PAL * 10) /
			(10 * (2 * (divisor + 7)) + 3);
	case ATARISIO_POKEY_DIVISOR_STANDARD:
		return 19200;
	case ATARISIO_POKEY_DIVISOR_2XSIO_XF551:
		return 38400;
	case ATARISIO_POKEY_DIVISOR_3XSIO:
		return 57600;
	default:
		return ATARISIO_ATARI_FREQUENCY_PAL / (2 * (divisor + 7));
	}
}

int UserspaceSIOWrapper::SendCommandFrame(Ext_SIO_parameters& params)
{
	int retry = 0;
	int ret = 0;

	uint8_t highspeed_mode = params.highspeed_mode;

	fCmdBuf[0] = params.device + params.unit - 1;
	fCmdBuf[1] = params.command;
	fCmdBuf[2] = params.aux1;
	fCmdBuf[3] = params.aux2;

	switch (params.highspeed_mode) {
	case ATARISIO_EXTSIO_SPEED_TURBO:
		fCmdBuf[3] |= 0x80;
		break;
	case ATARISIO_EXTSIO_SPEED_WARP:
		// warp speed only supported for read/write sector
		switch (params.command) {
		case 0x50:
		case 0x52:
		case 0x57:
			// set bit 5 of command
			fCmdBuf[1] |= 0x20;
			break;
		default:
			highspeed_mode = ATARISIO_EXTSIO_SPEED_NORMAL;
		}
		break;
	case ATARISIO_EXTSIO_SPEED_XF551:
		// XF551 sends response to format commands always in standard speed
		switch (params.command) {
		case 0x21:
		case 0x22:
			highspeed_mode = ATARISIO_EXTSIO_SPEED_NORMAL;
			break;
		default:
			break;
		}
		// set bit 7 of command
		fCmdBuf[1] |= 0x80;
		break;
	default:
		break;
	}

	fCmdBuf[4] = CalculateChecksum(fCmdBuf, 4);

	while (retry < eCommandFrameRetries) {
		if (params.highspeed_mode == ATARISIO_EXTSIO_SPEED_ULTRA) {
			SetBaudrateIfDifferent(fHighspeedBaudrate);
		} else {
			SetBaudrateIfDifferent(fStandardBaudrate);
		}
		tcflush(fDeviceFileNo, TCIOFLUSH);

		UTRACE_CMD_STATE("asserting command line");
		SetCommandLine(true);

		MicroSleep(eDelayT0);

		ret = TransmitBuf(fCmdBuf, eCmdRawLength, true);

		if (ret) {
			SetCommandLine(false);
			MilliSleep(20);
			goto next_retry;
		}

		if (highspeed_mode == ATARISIO_EXTSIO_SPEED_TURBO) {
			SetBaudrateIfDifferent(fHighspeedBaudrate);
		}

		tcflush(fDeviceFileNo, TCIFLUSH);

		MicroSleep(eDelayT1);

		UTRACE_CMD_STATE("de-asserting command line");
		SetCommandLine(false);

		ret = ReceiveByte(eDelayT2Max);
		if (ret < 0) {
			UTRACE_CMD_ERROR("waiting for command ACK returned %d\n", ret);
			ret = -ret;
			goto next_retry;
		}

		switch (highspeed_mode) {
		case ATARISIO_EXTSIO_SPEED_WARP:
		case ATARISIO_EXTSIO_SPEED_XF551:
			SetBaudrateIfDifferent(fHighspeedBaudrate);
			break;
		default:
			break;
		}

		UTRACE_CMD_DATA("got command ACK char %02x", ret);
		if (ret == cAckByte) {
			ret = 0;
			break;
		}

		if (ret == cNakByte) {
			ret = EATARISIO_COMMAND_NAK;
		} else {
			ret = EATARISIO_UNKNOWN_ERROR;
		}

	next_retry:
		retry++;

	}

	return ret;
}

int UserspaceSIOWrapper::InternalExtSIO(Ext_SIO_parameters& params)
{
	int ret = 0;
	int retry = 0;

	while (retry < eSIORetries) {
		ret = SendCommandFrame(params);
		UTRACE_1050_2_PC("SendCommandFrame returned %d", ret);
		if (ret) {
			goto retry_sio;
		}

		// send data
		if ((params.data_length > 0) &&
		    (params.direction & ATARISIO_EXTSIO_DIR_SEND)) {
			memcpy(fBuf, params.data_buffer, params.data_length);
			fBuf[params.data_length] = CalculateChecksum(fBuf, params.data_length);
			MicroSleep(eDelayT3Min);
			ret = TransmitBuf(fBuf, params.data_length+1, true);
			UTRACE_1050_2_PC("Sending %d data bytes returned %d",
				params.data_length, ret);
			if (ret) {
				goto retry_sio;
			}

			ret = ReceiveByte(eDelayT4Max + eReceiveHeadroom);
			UTRACE_1050_2_PC("Receive data ACK returned %d", ret);
			if (ret < 0) {
				ret = -ret;
				goto retry_sio;
			}
			switch (ret) {
			case cAckByte:
				ret = 0;
				break;
			case cNakByte:
				ret = EATARISIO_DATA_NAK;
				goto retry_sio;
			default:
				ret = EATARISIO_DATA_NAK;
				goto retry_sio;
			}
		}

		// wait for complete
		ret = ReceiveByte(params.timeout*1000*1000 + eReceiveHeadroom);
		UTRACE_1050_2_PC("Receive command complete returned %d", ret);

		if (ret < 0) {
			ret = -ret;
			goto retry_sio;
		}

		switch (ret) {
		case cAckByte:
			UTRACE_1050_2_PC("got ACK instead of complete");
			// fallthrough
		case cCompleteByte:
			ret = 0;
			break;
		case cErrorByte:
		default:
			ret = EATARISIO_COMMAND_COMPLETE_ERROR;
			goto retry_sio;
		}

		// receive data
		if ((params.data_length > 0) &&
		    (params.direction & ATARISIO_EXTSIO_DIR_RECV)) {
			ret = ReceiveBuf(params.data_length+1, eReceiveHeadroom);
			UTRACE_1050_2_PC("Receiving %d data bytes returned %d",
				params.data_length, ret);

			if (ret) {
				goto retry_sio;
			}
			memcpy(params.data_buffer, fBuf, params.data_length);

			if (BufChecksumOK(params.data_length)) {
				ret = 0;
			} else {
				UTRACE_1050_2_PC("Received data checksum error");
				ret = EATARISIO_DATA_NAK;
			}
		}

		if (ret == 0) {
			break;
		}
retry_sio:
		MilliSleep(20);
		retry++;
	}

	return ret;
}
