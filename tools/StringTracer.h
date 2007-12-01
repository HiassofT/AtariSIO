#ifndef STRINGTRACER_H
#define STRINGTRACER_H

/*
   StringTracer.h - collect trace output in a string

   Copyright (C) 2006 Matthias Reichl <hias@horus.com>

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
#include "DataContainer.h"

class StringTracer : public AbstractTracer {
public:
	StringTracer();

	virtual ~StringTracer() {}

	virtual void AddString(const char* string);

	virtual void FlushOutput() {}

	void ClearString();
	inline size_t GetStringLength() const;

	// attention: the string is _not_ null-terminated!
	// use GetStringLength() to get the actual size!
	inline const void* GetString() const;

protected:
	virtual void ReallyEndTraceLine();

private:
	RCPtr<DataContainer> fDataContainer;
};

inline size_t StringTracer::GetStringLength() const
{
	return fDataContainer->GetLength();
}

inline const void* StringTracer::GetString() const
{
	return fDataContainer->GetInternalDataPointer();
}

#endif
