#ifndef PRINTERHANDLER_H
#define PRINTERHANDLER_H

/*
   PrinterHandler - emulate Atari printer

   Copyright (C) 2004 Matthias Reichl <hias@horus.com>

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


#include <limits.h>
#include "AbstractSIOHandler.h"
#include "SIOTracer.h"
#include "Coprocess.h"

class PrinterHandler : public AbstractSIOHandler {
public:
	enum EEOLConversion { eRaw, eLF, eCR, eCRLF };

	PrinterHandler(const char* dest, EEOLConversion conv);
	virtual ~PrinterHandler();

	virtual int ProcessCommandFrame(SIO_command_frame& frame, const RCPtr<SIOWrapper>& wrapper);

	virtual bool IsPrinterHandler() const;

	virtual void ProcessDelayedTasks(bool isForced = false);

	virtual RCPtr<DiskImage> GetDiskImage();
	virtual RCPtr<const DiskImage> GetConstDiskImage() const;
	virtual bool EnableHighSpeed(bool);
	virtual bool SetHighSpeedParameters(unsigned int pokeyDivisor, unsigned int baudrate);
	virtual bool EnableXF551Mode(bool on);
	virtual bool EnableStrictFormatChecking(bool on);

	void SetEOLConversion(EEOLConversion conv);
	inline EEOLConversion GetEOLConversion() const;
	inline const char* GetFilename() const;

	enum EPrinterStatus {
		eStatusOK,
		eStatusSpawned,
		eStatusError
	};

	inline EPrinterStatus GetPrinterStatus() const;

private:
	bool SpawnProcess(const RCPtr<SIOWrapper>& wrapper);
	bool CloseProcess();

	void LogExternalReplies(bool block=false);

	EEOLConversion fEOLConversion;

	enum EDest { eFile, eProcess };
	EDest fDestination;

	char* fFilename;
	char* fPrintCommand;
	bool fProcessSpawned;

	FILE* fFile;
	Coprocess* fCoprocess;

	bool fWroteData;

	enum { eBufferLength = 512 };
	char fBuffer[eBufferLength];

	EPrinterStatus fPrinterStatus;
	SIOTracer* fTracer;
};

inline PrinterHandler::EEOLConversion PrinterHandler::GetEOLConversion() const
{
	return fEOLConversion;
}

inline const char* PrinterHandler::GetFilename() const
{
	return fFilename;
}

inline PrinterHandler::EPrinterStatus PrinterHandler::GetPrinterStatus() const
{
	return fPrinterStatus;
}

#endif
