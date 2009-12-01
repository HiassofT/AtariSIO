#ifndef DATACONTAINER_H
#define DATACONTAINER_H

/*
   DataContainer.h - storage class for misc binary data

   Copyright (C) 2005 Matthias Reichl <hias@horus.com>

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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "RefCounted.h"
#include "RCPtr.h"

class DataContainer : public RefCounted {
public:
	// the name must have length 4. If it is longer, it will be
	// truncated. If if's shorter, it will be padded with blanks
	DataContainer();
	virtual ~DataContainer();

	// append byte (8 bit)
	bool AppendByte(uint8_t byte);
	// append word (16 bit)
	bool AppendWord(unsigned short word);
	// append dword (32 bit)
	bool AppendDword(unsigned int dword);

	bool AppendBlock(const void* data, unsigned int len);
	bool AppendString(const char* data);

	inline size_t GetLength() const;

	bool GetDataBlock(void* data, size_t start, size_t len);

	inline const void* GetInternalDataPointer() const;

private:
	inline void PrepareForSize(size_t size);

	size_t fAllocatedSize;
	size_t fSize;

	uint8_t* fData;
};

inline void DataContainer::PrepareForSize(size_t size)
{
	if (size > fAllocatedSize) {
		fAllocatedSize = (size) * 2;
		fData = (uint8_t*) realloc(fData, fAllocatedSize);
	}
}

inline size_t DataContainer::GetLength() const
{
	return fSize;
}

inline const void* DataContainer::GetInternalDataPointer() const
{
	return fData;
}

#endif
