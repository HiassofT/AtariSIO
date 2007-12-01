#ifndef SIOTRACER_H
#define SIOTRACER_H

/*
   SIOTracer.h - simple trace / debug printer module

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

#include <stdio.h>

#ifndef WINVER
#include <sys/types.h>
#include "../driver/atarisio.h"
#endif

#include "AbstractTracer.h"
#include "RefCounted.h"
#include "RCPtr.h"

class SIOTracer {
private:
	class TracerEntry : public RefCounted {
	public:
		TracerEntry(const RCPtr<AbstractTracer>& tracer)
			: fTraceGroups(0),
			  fRealTracer(tracer)
		{}
		~TracerEntry() {}

		RCPtr<TracerEntry> fNext;

		unsigned int fTraceGroups;

		RCPtr<AbstractTracer> fRealTracer;
	};
public:

	static SIOTracer* GetInstance();

	void AddTracer(const RCPtr<AbstractTracer>& tracer);
	void RemoveTracer(const RCPtr<AbstractTracer>& tracer);
	void RemoveAllTracers();

	enum ETraceGroup {
		eTraceCommands = 1,
		eTraceUnhandeledCommands = 2,
		eTraceVerboseCommands = 4,
		eTraceDataBlocks = 8,
		eTraceAtpInfo = 16,
		eTraceInfo = 32,
		eTraceDebug = 64,
		eTraceImageStatus = 128,
		eTracePrinter = 256
	};

	enum EInfo {
		eInfoOK,
		eInfoWarning,
		eInfoError
	};

	void SetTraceGroup(ETraceGroup group, bool on, const RCPtr<AbstractTracer>& tracer = RCPtr<AbstractTracer>());

#ifndef WINVER
	void TraceCommandFrame(const SIO_command_frame &frame,
		const char *prefix = 0);

	void TraceUnhandeledCommandFrame(
		const SIO_command_frame &frame,
		const char *prefix = 0);

	void TraceCommandOK();
	void TraceCommandError(int returncode, unsigned char FDCstatus=255);

	void TraceDataBlock(
		const unsigned char* block,
		int len,
		const char *prefix = 0);

	void TraceDecodedPercomBlock(
		unsigned int driveno,
		const unsigned char* block /* 12 bytes long */,
		bool getBlock,
		bool XF551 = false);

	void TraceGetStatus(unsigned int driveno, bool XF551 = false);
	void TraceReadSector(unsigned int driveno, unsigned int sector, bool XF551 = false);
	void TraceWriteSector(unsigned int driveno, unsigned int sector, bool XF551 = false);
	void TraceWriteSectorVerify(unsigned int driveno, unsigned int sector, bool XF551 = false);
	void TraceFormatDisk(unsigned int driveno, bool XF551 = false);
	void TraceFormatEnhanced(unsigned int driveno, bool XF551 = false);
	void TraceGetSpeedByte(unsigned int driveno);
	void TraceGetSioCode(unsigned int driveno);
	void TraceGetSioCodeLength(unsigned int driveno);

	void TraceReadMyPicoDos(unsigned int driveno, unsigned int sector);

	void TraceGetPrinterStatus();
	void TraceWritePrinter();

	void TraceRemoteControlCommand();
	void TraceRemoteControlStatus();
	void TraceReadRemoteControlResult(unsigned int sector);

	void TraceApeSpecial(unsigned int driveno, const char* description = 0);
	void TraceRemoteControlGetTime();

	void TraceAtpDelay(unsigned int delay);
#endif
	void TraceString(ETraceGroup group, const char* format, ... )
		__attribute__ ((format (printf, 3, 4))) ;

	void TraceInfoString(EInfo infoType, const char* format, ... )
		__attribute__ ((format (printf, 3, 4))) ;

	void TraceDebugString(const char* format, ... )
		__attribute__ ((format (printf, 2, 3))) ;

	void IndicateDriveChanged(int drive);
	void IndicateDriveFormatted(int drive);
	void IndicateCwdChanged();
	void IndicatePrinterChanged();

protected:
	SIOTracer();

	~SIOTracer();

private:

	void IterStartTraceLine(ETraceGroup group);
	void IterEndTraceLine(ETraceGroup group);
	void IterFlushOutput(ETraceGroup group);

	void IterTraceString(ETraceGroup group, const char* string);
	void IterTraceHighlightString(ETraceGroup group, const char* string);
	void IterTraceOKString(ETraceGroup group, const char* string);
	void IterTraceDebugString(ETraceGroup group, const char* string);
	void IterTraceWarningString(ETraceGroup group, const char* string);
	void IterTraceErrorString(ETraceGroup group, const char* string);

	static SIOTracer* fInstance;

	unsigned int fTraceGroupsCache;

	RCPtr<TracerEntry> fTracerList;

	enum { eMaxStringLength = 1024 };

	char fString[eMaxStringLength];
};

inline SIOTracer* SIOTracer::GetInstance()
{
        if (fInstance == 0) {
                fInstance = new SIOTracer;
        }       
        return fInstance;
}

#define ALOG(x...) do { SIOTracer::GetInstance()->TraceInfoString(SIOTracer::eInfoOK, x); } while(0)
#define AWARN(x...) do { SIOTracer::GetInstance()->TraceInfoString(SIOTracer::eInfoWarning, x); } while(0)
#define AERROR(x...) do { SIOTracer::GetInstance()->TraceInfoString(SIOTracer::eInfoError, x); } while(0)

// some common warnings/errors
inline void LOG_SIO_CMD_ACK_FAILED()
{
	SIOTracer::GetInstance()->TraceInfoString(SIOTracer::eInfoWarning, "send command ACK failed");
}

inline void LOG_SIO_CMD_NAK_FAILED()
{
	SIOTracer::GetInstance()->TraceInfoString(SIOTracer::eInfoWarning, "send command NAK failed");
}

inline void LOG_SIO_COMPLETE_FAILED()
{
	SIOTracer::GetInstance()->TraceInfoString(SIOTracer::eInfoWarning, "send complete failed");
}

inline void LOG_SIO_ERROR_FAILED()
{
	SIOTracer::GetInstance()->TraceInfoString(SIOTracer::eInfoWarning, "send error failed");
}

inline void LOG_SIO_SEND_DATA_FAILED()
{
	SIOTracer::GetInstance()->TraceInfoString(SIOTracer::eInfoWarning, "send data frame failed");
}

inline void LOG_SIO_RECEIVE_DATA_FAILED()
{
	SIOTracer::GetInstance()->TraceInfoString(SIOTracer::eInfoWarning, "receive data frame failed");
}

inline void LOG_SIO_READ_ILLEGAL_SECTOR(unsigned int sec)
{
	SIOTracer::GetInstance()->TraceInfoString(SIOTracer::eInfoWarning, "illegal sector in read: %d", sec);
}

inline void LOG_SIO_WRITE_ILLEGAL_SECTOR(unsigned int sec)
{
	SIOTracer::GetInstance()->TraceInfoString(SIOTracer::eInfoWarning, "illegal sector in write: %d", sec);
}

#define LOG_SIO_MISC(x...) do { SIOTracer::GetInstance()->TraceInfoString(SIOTracer::eInfoWarning, x); } while(0)

#endif
