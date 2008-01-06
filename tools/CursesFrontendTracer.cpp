/*
   CursesFrontendTracer.cpp - send trace output to log window of
   curses frontend

   Copyright (C) 2003, 2004 Matthias Reichl <hias@horus.com>

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

#include "CursesFrontendTracer.h"
#include "CursesFrontend.h"

void CursesFrontendTracer::AddString(const char* string)
{
	fFrontend->AddLogString(string);
}

void CursesFrontendTracer::AddHighlightString(const char* string)
{
	fFrontend->AddLogHighlightString(string);
}

void CursesFrontendTracer::AddOKString(const char* string)
{
	fFrontend->AddLogOKString(string);
}

void CursesFrontendTracer::AddDebugString(const char* string)
{
	fFrontend->AddLogDebugString(string);
}

void CursesFrontendTracer::AddWarningString(const char* string)
{
	fFrontend->AddLogWarningString(string);
}

void CursesFrontendTracer::AddErrorString(const char* string)
{
	fFrontend->AddLogErrorString(string);
}

void CursesFrontendTracer::IndicateDriveChanged(int drive)
{
	fFrontend->DisplayDriveChangedStatus(DeviceManager::EDriveNumber(drive));
	fFrontend->DisplayDriveFilename(DeviceManager::EDriveNumber(drive));
}

void CursesFrontendTracer::IndicateDriveFormatted(int drive)
{
	fFrontend->DisplayDriveDensity(DeviceManager::EDriveNumber(drive));
	fFrontend->DisplayDriveSectors(DeviceManager::EDriveNumber(drive));
}

void CursesFrontendTracer::IndicatePrinterChanged()
{
	fFrontend->DisplayPrinterRunningStatus();
}

void CursesFrontendTracer::FlushOutput()
{
	fFrontend->UpdateScreen();
}

void CursesFrontendTracer::ReallyStartTraceLine()
{
	fFrontend->StartLogLine();
}

void CursesFrontendTracer::ReallyEndTraceLine()
{
	fFrontend->EndLogLine();
}

void CursesFrontendTracer::IndicateCwdChanged()
{
	fFrontend->InitTopLine();
}

void CursesFrontendTracer::IndicateCasStateChanged()
{
	fFrontend->DisplayCasState();
}

void CursesFrontendTracer::IndicateCasBlockChanged()
{
	fFrontend->DisplayCasBlock();
}
