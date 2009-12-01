/*
   DataContainer.cpp - storage class for misc binary data

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

#include <string.h>

#include "DataContainer.h"


DataContainer::DataContainer()
{
	fSize = 0;
	fAllocatedSize = 16;
	fData =(uint8_t*) malloc(fAllocatedSize);
}

DataContainer::~DataContainer()
{
	if (fData) {
		free(fData);
	}
}

bool DataContainer::AppendByte(uint8_t byte)
{
	PrepareForSize(fSize+1);
	fData[fSize++] = byte;
	return true;
}

bool DataContainer::AppendWord(unsigned short word)
{
	PrepareForSize(fSize+2);
	fData[fSize++] = word & 0xff;
	fData[fSize++] = (word>>8) & 0xff;
	return true;
}

bool DataContainer::AppendDword(unsigned int dword)
{
	PrepareForSize(fSize+4);
	fData[fSize++] = dword & 0xff;
	fData[fSize++] = (dword>>8) & 0xff;
	fData[fSize++] = (dword>>16) & 0xff;
	fData[fSize++] = (dword>>24) & 0xff;
	return true;
}

bool DataContainer::AppendBlock(const void* data, unsigned int len)
{
	PrepareForSize(fSize+len);
	memcpy(fData+fSize, data, len);
	fSize+=len;
	return true;
}

bool DataContainer::AppendString(const char* string)
{
	return AppendBlock(string, strlen(string));
}

bool DataContainer::GetDataBlock(void* data, size_t start, size_t len)
{
	if ((start >= fSize) || (start+len > fSize)) {
		return false;
	}
	memcpy(data, fData+start, len);
	return true;
}
