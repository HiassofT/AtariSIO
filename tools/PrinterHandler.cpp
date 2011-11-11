/*
   PrinterHandler - emulate Atari printer

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

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <stdlib.h>
#include "PrinterHandler.h"
#include "AtariDebug.h"
#include "MiscUtils.h"
#include "Error.h"

PrinterHandler::PrinterHandler(const char* dest, EEOLConversion conv)
	: fEOLConversion(conv),
	  fProcessSpawned(false),
	  fFile(NULL),
	  fCoprocess(NULL),
	  fWroteData(false),
	  fPrinterStatus(eStatusOK)
{
	fTracer = SIOTracer::GetInstance();
	if ((!dest) || (*dest == 0)) {
		Assert(false);
		throw ErrorObject("PrinterHandler needs a filename");
	}

	Assert(*dest);

	int len;
	if (*dest == '|') {
		if (dest[1] == 0) {
			throw ErrorObject("PrinterHandler: missing command");
		}
		len = strlen(dest);
		fFilename = new char[len+1];
		strcpy(fFilename, dest);
		fPrintCommand = fFilename+1;
		fDestination = eProcess;
	} else {
		fFile = fopen(dest, "w");
		if (fFile == NULL) {
			throw FileCreateError(dest);
		}
		char abspath[PATH_MAX];
		if (realpath(dest, abspath) == NULL) {
			fclose(fFile);
			throw FileCreateError(dest);
		}
		len = strlen(abspath);
		fFilename = new char[len+1];
		strcpy(fFilename, abspath);
		fDestination = eFile;
	}
}

PrinterHandler::~PrinterHandler()
{
	if (fDestination == eFile) {
		if (fFile) {
			fclose(fFile);
		}
		fFile = NULL;
	} else {
		if (fProcessSpawned) {
			CloseProcess();
		}
	}
	delete[] fFilename;
}

bool PrinterHandler::SpawnProcess(const RCPtr<SIOWrapper>& wrapper)
{
	if (fProcessSpawned) {
		Assert(false);
		return false;
	}
	fWroteData = false;
	try {
		int fd = wrapper->GetDeviceFD();
		fCoprocess = new Coprocess(fPrintCommand, &fd, 1);
	}
	catch (ErrorObject& err) {
		AERROR("%s", err.AsCString());
		fPrinterStatus = eStatusError;
		return false;
	}

	fPrinterStatus = eStatusSpawned;
	fProcessSpawned = true;

	/*
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 100*1000*1000;
	nanosleep(&ts, NULL);
	LogExternalReplies();
	*/
	return true;
}

bool PrinterHandler::CloseProcess()
{
	bool ret = true;
	LogExternalReplies();
	if (!fCoprocess->Close()) {
		AERROR("closing printer process failed");
		fPrinterStatus = eStatusError;
		ret = false;
	}
	LogExternalReplies(true);
	int i;
	if (!fCoprocess->Exit(&i)) {
		AERROR("shutting down printer process failed");
		fPrinterStatus = eStatusError;
		ret = false;
	}
	if (i != 0) {
		AWARN("printer process exited with status %d", i);
	}
	fPrinterStatus = eStatusOK;
	fProcessSpawned = false;
	return ret;
}

void PrinterHandler::ProcessDelayedTasks(bool isForced)
{
	EPrinterStatus oldStat = fPrinterStatus;
	if (fProcessSpawned && fWroteData) {
		fTracer->TraceString(SIOTracer::eTracePrinter, "[printer] flushing pending printer data");
		CloseProcess();
	}
	if (fDestination==eFile && isForced) {
		if (fFile) {
			if (fflush(fFile)) {
				fPrinterStatus = eStatusError;
			}
		} else {
			Assert(false);
		}
	}
	if (fPrinterStatus != oldStat) {
		fTracer->IndicatePrinterChanged();
	}
}

void PrinterHandler::LogExternalReplies(bool block)
{
	Assert(fProcessSpawned);
	int len;
	while (fCoprocess->ReadLine(fBuffer, eBufferLength, len, block)) {
		ALOG("[printer process] %s", fBuffer);
	}
}

int PrinterHandler::ProcessCommandFrame(SIO_command_frame& frame, const RCPtr<SIOWrapper>& wrapper)
{
	int ret=0;

	fTracer->TraceCommandFrame(frame);

	switch (frame.command) {
	case 0x53: {
		// get status

		if ((ret=wrapper->SendCommandACK())) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_CMD_ACK_FAILED();
			break;
		}

		unsigned int buflen = 4;
		uint8_t buf[buflen];
		const char* description = "[ get printer status ]";

		buf[0] = 0;
		buf[1] = 0;
		buf[2] = 20;	// timeout
		buf[3] = 0;

		fTracer->TraceCommandOK();
		fTracer->TraceGetPrinterStatus();
		fTracer->TraceDataBlock(buf, 4, description);

		if ((ret=wrapper->SendComplete())) {
			LOG_SIO_COMPLETE_FAILED();
			break;
		}

		if ((ret=wrapper->SendDataFrame(buf,4))) {
			LOG_SIO_SEND_DATA_FAILED();
			break;
		}

		break;
	}
	case 0x57: {
		// write to printer
		if ((ret=wrapper->SendCommandACK())) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_CMD_ACK_FAILED();
			break;
		}

		EPrinterStatus oldStat = fPrinterStatus;
		unsigned int buflen = 40;
		uint8_t buf[buflen];

		const char* description = "[ write printer data ]";

		if ((ret=wrapper->ReceiveDataFrame(buf, buflen))) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_RECEIVE_DATA_FAILED();
			break;
		}

		unsigned int len;
		for (len=0; (len < buflen) && (buf[len] != 155); len++) {
			fBuffer[len] = buf[len];
		}
		if (len < buflen) {
			switch (fEOLConversion) {
			case eRaw:
				fBuffer[len++] = 155; break;
			case eLF:
				fBuffer[len++] = 10; break;
			case eCR:
				fBuffer[len++] = 13; break;
			case eCRLF:
				fBuffer[len++] = 13;
				fBuffer[len++] = 10; break;
			}
		}
		bool new_spawn = false;
		bool spawn_ok = true;
		bool write_ok = false;
		if (fDestination == eFile) {
			fWroteData = true;
			if (fwrite(fBuffer, 1, len, fFile) != len) {
				write_ok = false;
				ret = AbstractSIOHandler::eWritePrinterError;
				fPrinterStatus = eStatusError;
			} else {
				write_ok = true;
				fPrinterStatus = eStatusOK;
			}
		} else {
			if (!fProcessSpawned) {
				new_spawn = true;
				spawn_ok = SpawnProcess(wrapper);
				if (!spawn_ok) {
					ret = AbstractSIOHandler::eExecError;
					fPrinterStatus = eStatusError;
				}
			}
			if (spawn_ok) {
				fWroteData = true;
				write_ok = fCoprocess->WriteData(fBuffer, len);
				if (!write_ok) {
					ret = AbstractSIOHandler::eWritePrinterError;
					fPrinterStatus = eStatusError;
				}
			}
		}
		if (write_ok) {
			fTracer->TraceCommandOK();
		} else {
			fTracer->TraceCommandError(ret);
		}
		fTracer->TraceWritePrinter();
		fTracer->TraceDataBlock(buf, buflen, description);
		if (fDestination == eProcess) {
			if (new_spawn) {
				if (spawn_ok) {
					fTracer->TraceString(SIOTracer::eTracePrinter, "[printer] spawned process \"%s\"", fPrintCommand);
				} else {
					AERROR("error spawning printer process \"%s\"", fPrintCommand);
					if (fProcessSpawned) {
						LogExternalReplies();
						CloseProcess();
					}
				}
			}
		}

		/*
		if (!write_ok) {
			AERROR("unable to write printer data");
		}
		*/

		if (fProcessSpawned) {
			LogExternalReplies();
		}

		if (oldStat != fPrinterStatus) {
			fTracer->IndicatePrinterChanged();
		}

		if (write_ok) {
			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}
		} else {
			if ((ret=wrapper->SendError())) {
				LOG_SIO_ERROR_FAILED();
				break;
			}
		}

		break;
	}
	default:
		if (wrapper->SendCommandNAK()) {
			LOG_SIO_CMD_NAK_FAILED();
		}
		ret = AbstractSIOHandler::eUnsupportedCommand;
		fTracer->TraceCommandError(ret);
		break;
	}

	return ret;
}

bool PrinterHandler::IsPrinterHandler() const
{
	return true;
}

RCPtr<DiskImage> PrinterHandler::GetDiskImage()
{
	return 0;
}

RCPtr<const DiskImage> PrinterHandler::GetConstDiskImage() const
{
	return 0;
}

bool PrinterHandler::EnableHighSpeed(bool)
{
	return false;
}

bool PrinterHandler::SetHighSpeedParameters(unsigned int, unsigned int)
{
	return false;
}

bool PrinterHandler::EnableXF551Mode(bool)
{
	return false;
}

bool PrinterHandler::EnableStrictFormatChecking(bool)
{
	return false;
}
