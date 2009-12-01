/*
   ChunkWriter.cpp - generate a chunk format based file

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

#include "ChunkWriter.h"
#include "Crc32.h"

ChunkWriter::ChunkWriter(const char* chunkName)
	: fIsOpen(true)
{
	unsigned int i,len;

	fAllocatedSize = 16;
	fData =(uint8_t*) malloc(fAllocatedSize);

	len = strlen(chunkName);
	if (len > 4) {
		len = 4;
	}
	for (i=0;i<len;i++) {
		fData[i] = chunkName[i];
	}
	for (i=len;i<4;i++) {
		fData[i] = ' ';
	}
	for (i=4;i<8;i++) {
		fData[i] = 0;
	}

	fSize = 8;
}

ChunkWriter::~ChunkWriter()
{
	if (fData) {
		free(fData);
	}
}

bool ChunkWriter::CloseChunk()
{
	if (fIsOpen) {
		fData[4] = (fSize-8) & 0xff;
		fData[5] = ( (fSize-8) >>8 ) & 0xff;
		fData[6] = ( (fSize-8) >>16 ) & 0xff;
		fData[7] = ( (fSize-8) >>24 ) & 0xff;
		if (fSize != fAllocatedSize) {
			fAllocatedSize = fSize;
			fData = (uint8_t*) realloc(fData, fAllocatedSize);
		}
		fIsOpen = false;
		return true;
	} else {
		return false;
	}
}

bool ChunkWriter::AppendByte(uint8_t byte)
{
	if (fIsOpen) {
		PrepareForSize(fSize+1);
		fData[fSize++] = byte;
		return true;
	} else {
		return false;
	}
}

bool ChunkWriter::AppendWord(uint16_t word)
{
	if (fIsOpen) {
		PrepareForSize(fSize+2);
		fData[fSize++] = word & 0xff;
		fData[fSize++] = (word>>8) & 0xff;
		return true;
	} else {
		return false;
	}
}

bool ChunkWriter::AppendDword(uint32_t dword)
{
	if (fIsOpen) {
		PrepareForSize(fSize+4);
		fData[fSize++] = dword & 0xff;
		fData[fSize++] = (dword>>8) & 0xff;
		fData[fSize++] = (dword>>16) & 0xff;
		fData[fSize++] = (dword>>24) & 0xff;
		return true;
	} else {
		return false;
	}
}

bool ChunkWriter::AppendBlock(const void* data, unsigned int len)
{
	if (fIsOpen) {
		PrepareForSize(fSize+len);
		memcpy(fData+fSize, data, len);
		fSize+=len;
		return true;
	} else {
		return false;
	}
}

bool ChunkWriter::AppendChunk(const RCPtr<ChunkWriter>& chunk)
{
	if (fIsOpen && !chunk->fIsOpen) {
		PrepareForSize(fSize+chunk->fSize);
		memcpy(fData+fSize, chunk->fData, chunk->fSize);
		fSize+=chunk->fSize;
		return true;
	} else {
		return false;
	}
}

uint32_t ChunkWriter::CalculateCRC32() const
{
	uint32_t crc = CRC32::CalcCRC32(0L, NULL, 0);
	crc = CRC32::CalcCRC32(crc, fData+8, fSize-8);
	return crc;
}

bool ChunkWriter::WriteToFile(RCPtr<FileIO>& f) const
{
	if (!fIsOpen) {
		if (f->WriteBlock(fData, fSize) == fSize) {
			return true;
		}
	}
	return false;
}
