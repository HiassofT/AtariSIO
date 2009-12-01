#ifndef CHUNKREADER_H
#define CHUNKREADER_H

/*
   ChunkReader.h - simple reader for chunk format based files

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

#include <stdio.h>
#include "RefCounted.h"
#include "RCPtr.h"
#include "FileIO.h"

class ChunkReader;

class ChunkReader : public RefCounted {
public:
	static RCPtr<ChunkReader> OpenChunkFile(RCPtr<FileIO>& f);

	// open sub-chunk at current position and move
	// current position to the end of the sub-chunk
	RCPtr<ChunkReader> OpenChunk();

	inline const char* GetChunkName() const;
	inline off_t GetChunkLength() const;
	inline off_t GetCurrentPosition() const;

	bool ReadByte(uint8_t& byte);
	bool ReadWord(uint16_t& word);
	bool ReadDword(uint32_t &dword);
	bool ReadBlock(void* buf, unsigned int len);

	uint32_t CalculateCRC32() const;
	bool CalculateCRC32(uint32_t &checksum, off_t start_pos, off_t end_pos) const;

private:
	ChunkReader(RCPtr<FileIO>& f,
		off_t start,
		off_t end,
		const char* name=0);

	virtual ~ChunkReader();

	inline void SeekToCurrentPos() const;

private:
	RCPtr<FileIO> fFile;

	off_t fChunkStart; // absolute file position (included)
	off_t fChunkEnd; // absolute file position (not included)
	off_t fCurrentPosition; // relative position within this chunk

	char *fName;
};

inline const char* ChunkReader::GetChunkName() const
{
	return fName;
}

inline off_t ChunkReader::GetChunkLength() const
{
	return fChunkEnd-fChunkStart;
}

inline off_t ChunkReader::GetCurrentPosition() const
{
	return fCurrentPosition;
}

inline void ChunkReader::SeekToCurrentPos() const
{
	fFile->Seek(fChunkStart + fCurrentPosition);
}

#endif
