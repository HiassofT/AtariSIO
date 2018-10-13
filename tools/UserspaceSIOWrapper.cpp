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

	struct termios2 tio;
	if (ioctl(fDeviceFileNo, TCGETS2, &tio)) {
		return false;
	}
	// equivalent of cfmakeraw 
	tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	tio.c_oflag &= ~OPOST;
	tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tio.c_cflag &= ~(CSIZE | CSTOPB | PARENB | CBAUD | CBAUD << LINUX_IBSHIFT);
	tio.c_cflag |= CS8 | CLOCAL | CREAD | B19200 | B19200 << LINUX_IBSHIFT;
	tio.c_cc[VMIN] = 0;
	tio.c_cc[VTIME] = 0;
	if (ioctl(fDeviceFileNo, TCSETS2, &tio)) {
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
	  fLastCommandOK(true)
{
	if (ioctl(fDeviceFileNo, TCGETS2, &fOriginalTermios)) {
		fDeviceFileNo = -1;
		throw new DeviceInitError("cannot get current serial port settings");
	}
	if (!InitSerialDevice()) {
		throw new DeviceInitError("cannot initialize serial port");
	}

	InitializeBaudrates();

	if (SetBaudrate(fStandardBaudrate)) {
		throw new DeviceInitError("cannot set standard baudrate");
	}
	tcflush(fDeviceFileNo, TCIOFLUSH);

	if (SetSIOServerMode(ESIOServerCommandLine::eCommandLine_RI)) {
		throw new DeviceInitError("cannot set SIO server mode");
	}
}

UserspaceSIOWrapper::~UserspaceSIOWrapper()
{
	if (fDeviceFileNo >= 0) {
		ioctl(fDeviceFileNo, TCSETS2, &fOriginalTermios);
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

int UserspaceSIOWrapper::SetCableType_1050_2_PC()
{
	TODO
}

int UserspaceSIOWrapper::SetCableType_APE_Prosystem()
{
	TODO
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
	default:
		fLastResult = 1;
	}

	// clear DTR and RTS to make autoswitching Atarimax interface work
	ClearControlLines();

	return fLastResult;
}

int UserspaceSIOWrapper::DirectSIO(SIO_parameters& /* params */)
{
	TODO
}

int UserspaceSIOWrapper::ExtSIO(Ext_SIO_parameters& /* params */)
{
	TODO
}

int UserspaceSIOWrapper::WaitForCommandFrame(int otherReadPollDevice)
{
	fd_set read_set;
	struct timeval tv;
	int maxfd;
	int flags;
	int sel;
	MiscUtils::TimestampType printerTimeout = MiscUtils::GetCurrentTimePlusSec(15);

	while (true) {
		tv.tv_sec = 0;
		tv.tv_usec = 100;

		FD_ZERO(&read_set);
		maxfd = -1;

		if (otherReadPollDevice >= 0) {
			maxfd = otherReadPollDevice;
			FD_SET(otherReadPollDevice, &read_set);
		}

		switch (fCommandReceiveState) {
		case eCommandSoftError:
			if (fLastCommandOK) {
				fLastCommandOK = false;
				fCommandReceiveState = eWaitCommandIdle;
				printerTimeout = MiscUtils::GetCurrentTimePlusSec(15);
				continue;
			}
			// fallthrough

		case eCommandHardError:
			fLastCommandOK = false;
			fCommandReceiveState = eWaitCommandIdle;
			printerTimeout = MiscUtils::GetCurrentTimePlusSec(15);
			TrySwitchbaud();
			continue;
			
		case eWaitCommandIdle:
			tcflush(fDeviceFileNo, TCIFLUSH);
			if (ioctl(fDeviceFileNo, TIOCMGET, &flags)) {
				AERROR("failed to read modem line status");
				break;
			}
			if (!(flags & fCommandLineMask)) {
				UTRACE_CMD_STATE("eWaitCommandIdle -> eWaitCommandAssert");
				fCommandReceiveState = eWaitCommandAssert;
				continue;
			}
			break;

		case eWaitCommandAssert:
			if (ioctl(fDeviceFileNo, TIOCMGET, &flags)) {
				AERROR("failed to read modem line status");
				break;
			}
			if (flags & fCommandLineMask) {
				UTRACE_CMD_STATE("eWaitCommandAssert -> eReceiveCommandFrame");
				fCommandReceiveState = eReceiveCommandFrame;
				fCommandReceiveCount = 0;
				fCommandFrameTimeout = MiscUtils::GetCurrentTimePlusMsec(15);
				continue;
			} else {
				tcflush(fDeviceFileNo, TCIFLUSH);
			}
			break;

		case eReceiveCommandFrame:
			tv.tv_usec = 1000;
			FD_SET(fDeviceFileNo, &read_set);
			if (fDeviceFileNo > maxfd) {
				maxfd = fDeviceFileNo;
			}
			break;
		case eWaitCommandDeassert:
			if (ioctl(fDeviceFileNo, TIOCMGET, &flags)) {
				AERROR("failed to read modem line status");
				break;
			}
			if (!(flags & fCommandLineMask)) {
				UTRACE_CMD_STATE("eWaitCommandDeassert -> eCommandOK");
				fCommandReceiveState = eCommandOK;
				fLastCommandOK = true;
				return 0;
			}
			break;
		case eCommandOK:
			UTRACE_CMD_STATE("eCommandOK -> eWaitCommandIdle");
			fCommandReceiveState = eWaitCommandIdle;
			continue;
		}

		sel = select(maxfd + 1, &read_set, NULL, NULL, &tv);

		if (sel < 0) {
			// error
			fCommandReceiveState = eCommandHardError;
			return 2;
		}

		if (sel > 0) {
			if (FD_ISSET(fDeviceFileNo, &read_set)) {
				int cnt = read(fDeviceFileNo, fCmdBuf + fCommandReceiveCount, 10);
				if (cnt < 0) {
					AERROR("failed to read command frame data");
					fCommandReceiveState = eCommandHardError;
					continue;
				}
				fCommandReceiveCount += cnt;
				UTRACE_CMD_DATA("got %d command frame bytes (%d total)",
					cnt, fCommandReceiveCount
				);
			}
			if (otherReadPollDevice >= 0 && FD_ISSET(otherReadPollDevice, &read_set)) {
				return 1;
			}
		}

		MiscUtils::TimestampType now = MiscUtils::GetCurrentTime();

		if (fCommandReceiveState == eReceiveCommandFrame) {
			if (fCommandReceiveCount == eCmdRawLength) {
				if (CmdBufChecksumOK()) {
					UTRACE_CMD_STATE("eReceiveCommandFrame -> eWaitCommandDeassert");
					UTRACE_CMD_BUF_OK;
					fCommandReceiveState = eWaitCommandDeassert;
					continue;
				} else {
					UTRACE_CMD_ERROR("command frame checksum error");
					UTRACE_CMD_BUF_ERROR;
					fCommandReceiveState = eCommandSoftError;
					continue;
				}
			}

			if (fCommandReceiveCount > eCmdRawLength) {
				UTRACE_CMD_ERROR("too many command frame data bytes (%d)", fCommandReceiveCount);
				fCommandReceiveState = eCommandHardError;
				continue;
			}

			if (now >= fCommandFrameTimeout) {
				UTRACE_CMD_ERROR("receive timeout expired, got %d bytes", fCommandReceiveCount);
				UTRACE_CMD_BUF_ERROR;
				if (fCommandReceiveCount < 4) {
					fCommandReceiveState = eCommandHardError;
				} else {
					fCommandReceiveState = eCommandSoftError;
				}
				continue;
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
		fCommandReceiveState = eWaitCommandAssert;
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

int UserspaceSIOWrapper::TransmitBuf(uint8_t* buf, unsigned int length)
{
	// timeout with 30% margin
	MiscUtils::TimestampType to = TimeForBytes(length) * 13 / 10 + eDelayT3;

	unsigned int pos = 0;
	int cnt;
	int sel;

	fd_set write_set;
	struct timeval tv;

	MiscUtils::TimestampType now = MiscUtils::GetCurrentTime();
	MiscUtils::TimestampType endTime = now + to;

	while (pos < length) {
		FD_ZERO(&write_set);
		FD_SET(fDeviceFileNo, &write_set);
		now = MiscUtils::GetCurrentTime();
		if (now >= endTime) {
			// DPRINTF("timeout in TransmitBuf(%d)", length);
			fLastResult = EATARISIO_COMMAND_TIMEOUT;
			return fLastResult;
		}
		MiscUtils::TimestampToTimeval(endTime - now, tv);
		sel = select(fDeviceFileNo + 1, NULL, &write_set, NULL, &tv);
		if (sel < 0) {
			// DPRINTF("select failed in TransmitBuf(%d): ", length, errno);
			fLastResult = EATARISIO_UNKNOWN_ERROR;
			return fLastResult;
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
	// DPRINTF("TransmitBuf(%d) OK", length);
	tcdrain(fDeviceFileNo);

	fLastResult = 0;
	return fLastResult;
}

void UserspaceSIOWrapper::WaitTransmitComplete()
{
	int cnt;
	unsigned int lsr;

	//tcdrain(fDeviceFileNo);
	cnt = 10;
	while (cnt--) {
		if (ioctl(fDeviceFileNo, TIOCSERGETLSR, &lsr)) {
			break;
		}
		if (lsr & TIOCSER_TEMT) {
			break;
		}
		NanoSleep(100000);
	}
	// one byte could still be in the transmitter holding register
	NanoSleep(TimeForBytes(1) * 1500);
}

int UserspaceSIOWrapper::TransmitByte(uint8_t byte, bool waitTransmit)
{
	int ret = TransmitBuf(&byte, 1);
	if (waitTransmit) {
		WaitTransmitComplete();
	}
	return ret;
}

int UserspaceSIOWrapper::TransmitBuf(unsigned int length)
{
	return TransmitBuf(fBuf, length);
}

int UserspaceSIOWrapper::ReceiveBuf(uint8_t* buf, unsigned int length, unsigned int additionalTimeout)
{
	// timeout with 30% margin
	MiscUtils::TimestampType to = TimeForBytes(length) * 13 / 10 + additionalTimeout;

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
			fLastResult = EATARISIO_COMMAND_TIMEOUT;
			return fLastResult;
		}
		MiscUtils::TimestampToTimeval(endTime - now, tv);
		sel = select(fDeviceFileNo + 1, &read_set, NULL, NULL, &tv);
		if (sel < 0) {
			fLastResult = EATARISIO_UNKNOWN_ERROR;
			return fLastResult;
		}
		if (sel == 1) {
			cnt = read(fDeviceFileNo, buf + pos, length - pos);
			if (cnt < 0) {
				return false;
			}
			pos += cnt;
		}
	}
	fLastResult = 0;
	return fLastResult;
}

int UserspaceSIOWrapper::ReceiveBuf(unsigned int length, unsigned int additionalTimeout)
{
	return ReceiveBuf(fBuf, length, additionalTimeout);
}

int UserspaceSIOWrapper::SendCommandACK()
{
	MicroSleep(eDelayT2);
	return TransmitByte('A', true);
}

int UserspaceSIOWrapper::SendCommandNAK()
{
	MicroSleep(eDelayT2);
	return TransmitByte('N');
}

int UserspaceSIOWrapper::SendDataACK()
{
	MicroSleep(eDelayT4);
	return TransmitByte('A', true);
}

int UserspaceSIOWrapper::SendDataNAK()
{
	MicroSleep(eDelayT4);
	return TransmitByte('N');
}

int UserspaceSIOWrapper::SendComplete()
{
	MicroSleep(eDelayT5);
	return TransmitByte('C');
}

int UserspaceSIOWrapper::SendError()
{
	MicroSleep(eDelayT5);
	return TransmitByte('E');
}

int UserspaceSIOWrapper::SendDataFrame(uint8_t* buf, unsigned int length)
{
	if (length > eMaxDataLength) {
		fLastResult = EATARISIO_ERROR_BLOCK_TOO_LONG;
		return fLastResult;
	}
	memcpy(fBuf, buf, length);
	fBuf[length] = CalculateChecksum(fBuf, length);

	WaitTransmitComplete();

	MicroSleep(eDataDelay);
	return TransmitBuf(fBuf, length+1);
}

int UserspaceSIOWrapper::ReceiveDataFrame(uint8_t* buf, unsigned int length)
{
	fLastResult = ReceiveBuf(fBuf, length+1, eDelayT3);
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
	return TransmitBuf(buf, length);
}

int UserspaceSIOWrapper::ReceiveRawFrame(uint8_t* buf, unsigned int length)
{
	return ReceiveBuf(buf, length, eDelayT3);
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
	struct termios2 tios2;
	UTRACE_BAUDRATE("change baudrate from %d to %d, now = %d", fBaudrate, baudrate, now);

	if (!now) {
		WaitTransmitComplete();
	}

	int fLastResult = ioctl(fDeviceFileNo, TCGETS2, &tios2);
	if (fLastResult < 0) {
		return fLastResult;
	}
	tios2.c_cflag &= ~(CBAUD | CBAUD << LINUX_IBSHIFT);
	switch (baudrate) {
#if 0
	case 19200:
		tios2.c_cflag |= B19200 | B19200 << LINUX_IBSHIFT;
		break;
	case 38400:
		tios2.c_cflag |= B38400 | B38400 << LINUX_IBSHIFT;
		break;
	case 57600:
		tios2.c_cflag |= B57600 | B57600 << LINUX_IBSHIFT;
		break;
#endif
	default:
		tios2.c_cflag |= BOTHER | BOTHER << LINUX_IBSHIFT;
		break;
	};
	tios2.c_ispeed = baudrate;
	tios2.c_ospeed = baudrate;

	fLastResult = ioctl(fDeviceFileNo, now ? TCSETS2 : TCSETSW2, &tios2);
	if (!fLastResult) {
		fBaudrate = baudrate;
	}
	return fLastResult;
}

void UserspaceSIOWrapper::TrySwitchbaud()
{
	if (fDoAutobaud) {
		if (fBaudrate == fStandardBaudrate) {
			SetBaudrate(fHighspeedBaudrate);
		} else {
			SetBaudrate(fStandardBaudrate);
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
		return 125000;
	case 1:
		return 110500;
	case 2:
		return 98500;
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
