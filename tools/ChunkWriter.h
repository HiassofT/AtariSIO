#ifndef CHUNKWRITER_H
#define CHUNKWRITER_H

/*
   ChunkWriter.h - generate a chunk format based file

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

#include <stdlib.h>
#include <stdio.h>

#include "RefCounted.h"
#include "RCPtr.h"
#include "FileIO.h"

class ChunkWriter;

class ChunkWriter : public RefCounted {
public:
	// the name must have length 4. If it is longer, it will be
	// truncated. If if's shorter, it will be padded with blanks
	ChunkWriter(const char* chunkName);
	virtual ~ChunkWriter();

	// append byte (8 bit)
	bool AppendByte(uint8_t byte);
	// append word (16 bit)
	bool AppendWord(uint16_t word);
	// append dword (32 bit)
	bool AppendDword(uint32_t dword);

	bool AppendBlock(const void* data, unsigned int len);
	bool AppendChunk(const RCPtr<ChunkWriter>& chunk);

	// set the length field of the chunk and free any
	// unused memory.
	// Note: after closing a chunk, no further data may be appended
	// to it!
	bool CloseChunk();

	// calculates checksum of the data part of this chunk
	uint32_t CalculateCRC32() const;

	bool WriteToFile(RCPtr<FileIO>& f) const;

private:
	inline void PrepareForSize(size_t size);

	bool fIsOpen;
	size_t fAllocatedSize;
	size_t fSize;

	uint8_t* fData;
};

inline void ChunkWriter::PrepareForSize(size_t size)
{
	if (size > fAllocatedSize) {
		fAllocatedSize = (size) * 2;
		fData = (uint8_t*) realloc(fData, fAllocatedSize);
	}
}

#endif
