#ifndef ABSTRACTTRACER_H
#define ABSTRACTTRACER_H

/*
   AbstractTracer.h - generic base class for all tracer instances

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

#include "RefCounted.h"

class AbstractTracer : public RefCounted {
public:
	AbstractTracer()
		: fTraceLineStarted(false)
	{}
	virtual ~AbstractTracer()
	{}

	virtual void StartTraceLine()
	{
		if (!fTraceLineStarted) {
			fTraceLineStarted = true;
			ReallyStartTraceLine();
		}
	}
	virtual void EndTraceLine()
	{
		if (fTraceLineStarted) {
			fTraceLineStarted = false;
			ReallyEndTraceLine();
		}
	}

	virtual void AddString(const char* string) = 0;

	virtual void AddHighlightString(const char* string)
		{ AddString(string); }

	virtual void AddOKString(const char* string)
		{ AddString(string); }
	virtual void AddDebugString(const char* string)
		{ AddString(string); }
	virtual void AddWarningString(const char* string)
		{ AddString(string); }
	virtual void AddErrorString(const char* string)
		{ AddString(string); }

	virtual void FlushOutput() { }

	virtual void IndicateDriveChanged(int /*driveno*/) { }
	virtual void IndicateDriveFormatted(int /*driveno*/) { }
	virtual void IndicateCwdChanged() { }
	virtual void IndicatePrinterChanged() { }

	virtual void IndicateCasStateChanged() { }
	virtual void IndicateCasBlockChanged() { }

protected:
	virtual void ReallyStartTraceLine()
	{}
	virtual void ReallyEndTraceLine()
	{ AddString("\n"); }

private:
	bool fTraceLineStarted;
};

#endif
