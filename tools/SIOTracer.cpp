/*
   SIOTracer.cpp - simple trace / debug printer module

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

#include <stdarg.h>
#include <string.h>

#include "SIOTracer.h"

#if !defined(WINVER) && !defined(POSIXVER)
#include "AbstractSIOHandler.h"
#endif

#include "AtariDebug.h"
#include "winver.h"

SIOTracer* SIOTracer::fInstance = 0;

SIOTracer::SIOTracer()
	: fTraceGroupsCache(0),
	  fTracerList(0)
{
}

SIOTracer::~SIOTracer()
{
	fTracerList = 0;
}

void SIOTracer::AddTracer(const RCPtr<AbstractTracer>& tracer)
{
	if (tracer.IsNull()) {
		DPRINTF("call to AddTracer with NULL-pointer!");
		return;
	}

	RCPtr<TracerEntry> e = new TracerEntry(tracer);
	e->fNext = fTracerList;
	fTracerList = e;
}

void SIOTracer::RemoveTracer(const RCPtr<AbstractTracer>& tracer)
{
	if (tracer.IsNull()) {
		DPRINTF("call to RemoveTracer with NULL-pointer!");
		return;
	}

	RCPtr<TracerEntry> prev = 0;
	RCPtr<TracerEntry> e = fTracerList;

	fTraceGroupsCache = 0;
	while (e.IsNotNull()) {
		if (e->fRealTracer == tracer) {
			if (prev.IsNotNull()) {
				prev->fNext = e->fNext;
			} else {
				fTracerList = e->fNext;
			}
		} else {
			fTraceGroupsCache |= e->fTraceGroups;
		}
		prev = e;
		e = e->fNext;
	}
}

void SIOTracer::RemoveAllTracers()
{
	fTracerList = 0;
	fTraceGroupsCache = 0;
}

void SIOTracer::SetTraceGroup(ETraceGroup group, bool on, const RCPtr<AbstractTracer>& tracer)
{
	RCPtr<TracerEntry> e = fTracerList;

	fTraceGroupsCache = 0;

	while (e.IsNotNull()) {
		if ( (tracer.IsNull()) || (tracer == e->fRealTracer) ) {
			if (on) {
				e->fTraceGroups |= (group);
			} else {
				e->fTraceGroups &= ~group;
			}
		}
		fTraceGroupsCache |= e->fTraceGroups;
		e = e->fNext;
	}
}

void SIOTracer::IterStartTraceLine(ETraceGroup group)
{
	RCPtr<TracerEntry> e = fTracerList;
	while (e.IsNotNull()) {
		if (e->fTraceGroups & group) {
			e->fRealTracer->StartTraceLine();
		}
		e = e->fNext;
	}
}

void SIOTracer::IterEndTraceLine(ETraceGroup group)
{
	RCPtr<TracerEntry> e = fTracerList;
	while (e.IsNotNull()) {
		if (e->fTraceGroups & group) {
			e->fRealTracer->EndTraceLine();
		}
		e = e->fNext;
	}
}

void SIOTracer::IterFlushOutput(ETraceGroup group)
{
	RCPtr<TracerEntry> e = fTracerList;
	while (e.IsNotNull()) {
		if (e->fTraceGroups & group) {
			e->fRealTracer->FlushOutput();
		}
		e = e->fNext;
	}
}

void SIOTracer::IterTraceString(ETraceGroup group, const char* string)
{
	RCPtr<TracerEntry> e = fTracerList;
	while (e.IsNotNull()) {
		if (e->fTraceGroups & group) {
			e->fRealTracer->AddString(string);
		}
		e = e->fNext;
	}
}

void SIOTracer::IterTraceHighlightString(ETraceGroup group, const char* string)
{
	RCPtr<TracerEntry> e = fTracerList;
	while (e.IsNotNull()) {
		if (e->fTraceGroups & group) {
			e->fRealTracer->AddHighlightString(string);
		}
		e = e->fNext;
	}
}

void SIOTracer::IterTraceOKString(ETraceGroup group, const char* string)
{
	RCPtr<TracerEntry> e = fTracerList;
	while (e.IsNotNull()) {
		if (e->fTraceGroups & group) {
			e->fRealTracer->AddOKString(string);
		}
		e = e->fNext;
	}
}

void SIOTracer::IterTraceDebugString(ETraceGroup group, const char* string)
{
	RCPtr<TracerEntry> e = fTracerList;
	while (e.IsNotNull()) {
		if (e->fTraceGroups & group) {
			e->fRealTracer->AddDebugString(string);
		}
		e = e->fNext;
	}
}

void SIOTracer::IterTraceWarningString(ETraceGroup group, const char* string)
{
	RCPtr<TracerEntry> e = fTracerList;
	while (e.IsNotNull()) {
		if (e->fTraceGroups & group) {
			e->fRealTracer->AddWarningString(string);
		}
		e = e->fNext;
	}
}

void SIOTracer::IterTraceErrorString(ETraceGroup group, const char* string)
{
	RCPtr<TracerEntry> e = fTracerList;
	while (e.IsNotNull()) {
		if (e->fTraceGroups & group) {
			e->fRealTracer->AddErrorString(string);
		}
		e = e->fNext;
	}
}

#if !defined(WINVER) && !defined(POSIXVER)

void SIOTracer::TraceCommandFrame(
	const SIO_command_frame &frame,
	const char *prefix)
{
	if ((fTraceGroupsCache & eTraceCommands)) {
		IterStartTraceLine(eTraceCommands);
		
		if (prefix) {
			snprintf(fString, eMaxStringLength, "%s ", prefix);
			IterTraceString(eTraceCommands, fString);
		}
		IterTraceString(eTraceCommands, "command frame [ ");
		snprintf(fString, eMaxStringLength, "%02x %02x %02x %02x",
			frame.device_id, frame.command, frame.aux1, frame.aux2);
		IterTraceHighlightString(eTraceCommands, fString);
		IterTraceString(eTraceCommands, " ]  ");
	}
}

void SIOTracer::TraceUnhandeledCommandFrame(
	const SIO_command_frame &frame,
	const char *prefix)
{
	if (fTraceGroupsCache & eTraceUnhandeledCommands) {
		IterStartTraceLine(eTraceUnhandeledCommands);
		
		if (prefix) {
			snprintf(fString, eMaxStringLength, "%s ", prefix);
			IterTraceString(eTraceUnhandeledCommands, fString);
		}

		IterTraceString(eTraceCommands, "command frame [ ");
		snprintf(fString, eMaxStringLength, "%02x %02x %02x %02x",
			frame.device_id, frame.command, frame.aux1, frame.aux2);
		IterTraceHighlightString(eTraceCommands, fString);
		IterTraceString(eTraceCommands, " ]  ");

		IterTraceWarningString(eTraceUnhandeledCommands,"unhandeled");
		//IterTraceString(eTraceUnhandeledCommands," (no disk installed)");
		IterEndTraceLine(eTraceUnhandeledCommands);
		IterFlushOutput(eTraceUnhandeledCommands);
	}
}

void SIOTracer::TraceCommandOK()
{
	if (fTraceGroupsCache & eTraceCommands) {
		IterTraceOKString(eTraceCommands, "OK");
		IterEndTraceLine(eTraceCommands);
		IterFlushOutput(eTraceCommands);
	}
}

void SIOTracer::TraceCommandError(int returncode, uint8_t FDC_status)
{
	if (fTraceGroupsCache & eTraceCommands) {
		switch (returncode) {
		case AbstractSIOHandler::eWriteProtected:
			IterTraceErrorString(eTraceCommands, "ERROR:");
			IterTraceString(eTraceCommands, " write protected");
			break;
		case AbstractSIOHandler::eUnsupportedCommand:
			IterTraceErrorString(eTraceCommands, "ERROR:");
			IterTraceString(eTraceCommands, " unsupported command");
			break;
		case AbstractSIOHandler::eInvalidPercomConfig:
			IterTraceErrorString(eTraceCommands, "ERROR:");
			IterTraceString(eTraceCommands, " invalid percom config");
			break;
		case AbstractSIOHandler::eIllegalSectorNumber:
			IterTraceErrorString(eTraceCommands, "ERROR:");
			IterTraceString(eTraceCommands, " illegal sector number");
			break;
		case AbstractSIOHandler::eImageError:
			IterTraceErrorString(eTraceCommands, "ERROR:");
			IterTraceString(eTraceCommands, " disk image error");
			break;
		case AbstractSIOHandler::eAtpSectorStatus:
			IterTraceErrorString(eTraceCommands, "ERROR:");
			snprintf(fString, eMaxStringLength, " ATP sector status %02x", FDC_status);
			IterTraceString(eTraceCommands, fString);
			break;
		case AbstractSIOHandler::eAtpWrongSpeed:
			IterTraceWarningString(eTraceCommands, "ignored:");
			IterTraceString(eTraceCommands, " only supporting 19200 bit/sec");
			break;
		case AbstractSIOHandler::eExecError:
			IterTraceErrorString(eTraceCommands, "ERROR:");
			IterTraceString(eTraceCommands, " spawning external process failed");
			break;
		case AbstractSIOHandler::eWritePrinterError:
			IterTraceErrorString(eTraceCommands, "ERROR:");
			IterTraceString(eTraceCommands, " writing printer data failed");
			break;
		case AbstractSIOHandler::eRemoteControlError:
			IterTraceErrorString(eTraceCommands, "ERROR:");
			IterTraceString(eTraceCommands, " illegal remote control frame");
			break;
		default:
			IterTraceErrorString(eTraceCommands, "ERROR:");
			snprintf(fString, eMaxStringLength, " %d\n", returncode);
			IterTraceString(eTraceCommands, fString);
		}
		IterEndTraceLine(eTraceCommands);
		IterFlushOutput(eTraceCommands);
	}
}

void SIOTracer::TraceDataBlock(
	const uint8_t* block,
	unsigned int len,
	const char *prefix)
{
	if (fTraceGroupsCache & eTraceDataBlocks) {
		IterStartTraceLine(eTraceDataBlocks);
		if (prefix) {
			snprintf(fString, eMaxStringLength, "%s ", prefix);
			IterTraceString(eTraceDataBlocks, fString);
		}
		snprintf(fString, eMaxStringLength, "data block length = %d bytes", (int)len);
		IterTraceString(eTraceDataBlocks, fString);
		IterEndTraceLine(eTraceDataBlocks);

		size_t i,j;
		for (j=0;j<len;j += 16) {
			IterStartTraceLine(eTraceDataBlocks);
			for (i=0;i<16;i++) {
				if (i+j < len) {
					snprintf(fString, eMaxStringLength, "%02x ",block[i+j]);
				} else {
					strcpy(fString,"   ");
				}
				IterTraceString(eTraceDataBlocks, fString);
			}
			strcpy(fString,"  ");
			IterTraceString(eTraceDataBlocks, fString);

			for (i=0;i<16;i++) {
				fString[0] = ' ';
				fString[1] = 0;
				if (i+j < len) {
					fString[0] = '.';
					if (block[i+j] >= 32 && block[i+j] <= 126) {
						fString[0] = block[i+j];
					}
				}
				IterTraceString(eTraceDataBlocks, fString);
			}
			
			IterEndTraceLine(eTraceDataBlocks);
		}
		IterFlushOutput(eTraceDataBlocks);
	}
}

static inline const char* is_xf551(bool xf551_flag)
{
	return xf551_flag ? " XF551" : "";
}

void SIOTracer::TraceDecodedPercomBlock(unsigned int driveno, const uint8_t* block, bool get, bool XF551)
{
	ETraceGroup group = eTraceVerboseCommands;
	if (fTraceGroupsCache & group) {
		IterStartTraceLine(group);
		unsigned int sides = block[4] + 1;
		unsigned int tracks = block[0];
		unsigned int sectors = (block[2]<<8) + block[3];
		unsigned int total = sides * tracks * sectors;
		unsigned int seclen = (block[6]<<8) + block[7];
		uint8_t dens = block[5];

		snprintf(fString, eMaxStringLength, "D%d: %s percom block%s : ",
			driveno,
			(get ? "get" : "put"),
			is_xf551(XF551));
		IterTraceString(group, fString);

		if ( (sides == 1) && (sectors == 18) && (tracks == 40) && (seclen == 128) && (dens == 0) ) {
			IterTraceString(group, "90k (SD)");
		} else if ( (sides == 1) && (sectors == 26) && (tracks == 40) && (seclen == 128) && (dens == 4) ) {
			IterTraceString(group, "130k (ED)");
		} else if ( (sides == 1) && (sectors == 18) && (tracks == 40) && (seclen == 256) && (dens == 4) ) {
			IterTraceString(group, "180k (DD)");
		} else if ( (sides == 2) && (sectors == 18) && (tracks == 40) && (seclen == 256) && (dens == 4) ) {
			IterTraceString(group, "360k (QD)");
		} else {
			snprintf(fString, eMaxStringLength, "%d H %d C %d S = %d sec %d bytes/sec ",
				sides, tracks, sectors, total, seclen);
			IterTraceString(group, fString);

			switch (dens) {
			case 0:
				IterTraceString(group, "FM");
				break;
			case 4:
				IterTraceString(group, "MFM");
				break;
			default:
				snprintf(fString, eMaxStringLength, "illegal density %d", dens);
				IterTraceString(group, fString);
				break;
			}
		}
		IterEndTraceLine(group);
		IterFlushOutput(group);
	}
}

void SIOTracer::TraceGetStatus(unsigned int driveno, bool XF551)
{
	TraceString(eTraceVerboseCommands, "D%d: get status%s", driveno, is_xf551(XF551));
}

void SIOTracer::TraceReadSector(unsigned int driveno, unsigned int sector, bool XF551)
{
	TraceString(eTraceVerboseCommands, "D%d: read sector%s %d", driveno, is_xf551(XF551), sector);
}

void SIOTracer::TraceWriteSector(unsigned int driveno, unsigned int sector, bool XF551)
{
	TraceString(eTraceVerboseCommands, "D%d: write sector%s %d", driveno, is_xf551(XF551), sector);
}

void SIOTracer::TraceWriteSectorVerify(unsigned int driveno, unsigned int sector, bool XF551)
{
	TraceString(eTraceVerboseCommands, "D%d: write (and verify)%s sector %d", driveno, is_xf551(XF551), sector);
}

void SIOTracer::TraceFormatDisk(unsigned int driveno, bool XF551)
{
	TraceString(eTraceVerboseCommands, "D%d: format disk%s", driveno, is_xf551(XF551));
}

void SIOTracer::TraceFormatEnhanced(unsigned int driveno, bool XF551)
{
	TraceString(eTraceVerboseCommands, "D%d: format enhanced density%s", driveno, is_xf551(XF551));
}

void SIOTracer::TraceGetSpeedByte(unsigned int driveno)
{
	TraceString(eTraceVerboseCommands, "D%d: get speed byte", driveno);
}

void SIOTracer::TraceApeSpecial(unsigned int driveno, const char* description)
{
	TraceString(eTraceVerboseCommands, "D%d: APE %s", driveno, description ? description : "[unknown]");
}

void SIOTracer::TraceGetSioCode(unsigned int driveno)
{
	TraceString(eTraceVerboseCommands, "D%d: get SIO code", driveno);
}

void SIOTracer::TraceGetSioCodeLength(unsigned int driveno)
{
	TraceString(eTraceVerboseCommands, "D%d: get SIO code length", driveno);
}

void SIOTracer::TraceReadMyPicoDos(unsigned int driveno, unsigned int sector)
{
	TraceString(eTraceVerboseCommands, "D%d: read MyPicoDos sector %d", driveno, sector);
}

void SIOTracer::TraceGetPrinterStatus()
{
	TraceString(eTraceVerboseCommands, "P: get status");
}

void SIOTracer::TraceWritePrinter()
{
	TraceString(eTraceVerboseCommands, "P: write data");
}

void SIOTracer::TraceRemoteControlCommand()
{
	TraceString(eTraceVerboseCommands, "remote control command");
}

void SIOTracer::TraceRemoteControlStatus()
{
	TraceString(eTraceVerboseCommands, "remote control status");
}

void SIOTracer::TraceReadRemoteControlResult(unsigned int sector)
{
	TraceString(eTraceVerboseCommands, "read remote control result %d", sector);
}

void SIOTracer::TraceRemoteControlGetTime()
{
	TraceString(eTraceVerboseCommands, "remote control get time");
}


void SIOTracer::TraceAtpDelay(unsigned int delay)
{
	if (fTraceGroupsCache & eTraceAtpInfo) {
		IterStartTraceLine(eTraceAtpInfo);
		snprintf(fString, eMaxStringLength, "ATP delay: %u\n", delay);
		IterTraceString(eTraceAtpInfo, fString);
		IterEndTraceLine(eTraceAtpInfo);
		IterFlushOutput(eTraceAtpInfo);
	}
}
#endif

void SIOTracer::TraceString(ETraceGroup group, const char* format, ...)
{
	if (fTraceGroupsCache & group) {
		va_list arg;
		va_start(arg, format);

		vsnprintf(fString, eMaxStringLength, format, arg);

		va_end(arg);

		IterStartTraceLine(group);
		switch (group) {
		case eTraceWarning:
			IterTraceWarningString(group,"Warning: ");
			break;
		case eTraceError:
			IterTraceErrorString(group,"Error: ");
			break;
		case eTraceDebug:
			IterTraceDebugString(group,"Debug: ");
			break;
		default:
			break;
		}
		IterTraceString(group, fString);
		IterEndTraceLine(group);
		IterFlushOutput(group);
	}
}

void SIOTracer::IndicateDriveChanged(unsigned int drive)
{
	if (fTraceGroupsCache & eTraceImageStatus) {
		RCPtr<TracerEntry> e = fTracerList;
		while (e.IsNotNull()) {
			if (e->fTraceGroups & eTraceImageStatus) {
				e->fRealTracer->IndicateDriveChanged(drive);
				e->fRealTracer->FlushOutput();
			}
			e = e->fNext;
		}
	}
}

void SIOTracer::IndicateDriveFormatted(unsigned int drive)
{
	if (fTraceGroupsCache & eTraceImageStatus) {
		RCPtr<TracerEntry> e = fTracerList;
		while (e.IsNotNull()) {
			if (e->fTraceGroups & eTraceImageStatus) {
				e->fRealTracer->IndicateDriveFormatted(drive);
				e->fRealTracer->FlushOutput();
			}
			e = e->fNext;
		}
	}
}

void SIOTracer::IndicateCwdChanged()
{
	if (fTraceGroupsCache & eTraceImageStatus) {
		RCPtr<TracerEntry> e = fTracerList;
		while (e.IsNotNull()) {
			if (e->fTraceGroups & eTraceImageStatus) {
				e->fRealTracer->IndicateCwdChanged();
				e->fRealTracer->FlushOutput();
			}
			e = e->fNext;
		}
	}
}

void SIOTracer::IndicateCasStateChanged()
{
	if (fTraceGroupsCache & eTraceImageStatus) {
		RCPtr<TracerEntry> e = fTracerList;
		while (e.IsNotNull()) {
			if (e->fTraceGroups & eTraceImageStatus) {
				e->fRealTracer->IndicateCasStateChanged();
				e->fRealTracer->FlushOutput();
			}
			e = e->fNext;
		}
	}
}

void SIOTracer::IndicateCasBlockChanged()
{
	if (fTraceGroupsCache & eTraceImageStatus) {
		RCPtr<TracerEntry> e = fTracerList;
		while (e.IsNotNull()) {
			if (e->fTraceGroups & eTraceImageStatus) {
				e->fRealTracer->IndicateCasBlockChanged();
				e->fRealTracer->FlushOutput();
			}
			e = e->fNext;
		}
	}
}

void SIOTracer::IndicatePrinterChanged()
{
	if (fTraceGroupsCache & eTraceImageStatus) {
		RCPtr<TracerEntry> e = fTracerList;
		while (e.IsNotNull()) {
			if (e->fTraceGroups & eTraceImageStatus) {
				e->fRealTracer->IndicatePrinterChanged();
				e->fRealTracer->FlushOutput();
			}
			e = e->fNext;
		}
	}
}

