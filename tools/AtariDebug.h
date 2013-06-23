#ifndef ATARIDEBUG_H
#define ATARIDEBUG_H

/*
   AtariDebug.h - some short helper routines for debugging

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
#include <assert.h>
#include "SIOTracer.h"

#ifdef ATARISIO_DEBUG
#define DPRINTF(x...) do { SIOTracer::GetInstance()->TraceString(SIOTracer::eTraceDebug, x); } while(0)
#define Assert(expr) assert(expr)
#define AssertMsg(expr,msg) \
	if (! (expr) ) { \
		fprintf(stderr,"Assertion failed: %s\n", msg); \
		fflush(stderr); \
		assert(expr); \
	}
#else
#define DPRINTF(x...) do { } while(0)
#define Assert(expr) do { } while(0)
#define AssertMsg(expr,msg) do { } while(0)
#endif

#define TRACE(x...) do { SIOTracer::GetInstance()->TraceString(SIOTracer::eTraceInfo, x); } while(0)

#endif
