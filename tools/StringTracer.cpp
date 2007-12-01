/*
   StringTracer.cpp - collect trace output in a string

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

#include "StringTracer.h"
#include "Error.h"

StringTracer::StringTracer()
	: fDataContainer(new DataContainer)
{
}

void StringTracer::ClearString()
{
	fDataContainer = new DataContainer;
}

void StringTracer::AddString(const char* string)
{
	fDataContainer->AppendString(string);
}

void StringTracer::ReallyEndTraceLine()
{
	fDataContainer->AppendByte('\n');
}
