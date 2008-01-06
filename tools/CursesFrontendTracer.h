#ifndef CURSESFRONTENDTRACER_H
#define CURSESFRONTENDTRACER_H

/*
   CursesFrontendTracer.h - send trace output to log window of
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

#include "AbstractTracer.h"

class CursesFrontend;

class CursesFrontendTracer : public AbstractTracer {
public:
	CursesFrontendTracer(CursesFrontend* fe)
		: fFrontend(fe)
	{ }

	virtual ~CursesFrontendTracer()
	{ }

	virtual void AddString(const char* string);
	virtual void AddHighlightString(const char* string);
	virtual void AddOKString(const char* string);
	virtual void AddDebugString(const char* string);
	virtual void AddWarningString(const char* string);
	virtual void AddErrorString(const char* string);
	virtual void FlushOutput();

	virtual void IndicateDriveChanged(int drive);
	virtual void IndicateDriveFormatted(int drive);
	virtual void IndicateCwdChanged();
	virtual void IndicatePrinterChanged();

	virtual void IndicateCasStateChanged();
	virtual void IndicateCasBlockChanged();


protected:
	virtual void ReallyStartTraceLine();
	virtual void ReallyEndTraceLine();

private:
	CursesFrontend* fFrontend;
};

#endif
