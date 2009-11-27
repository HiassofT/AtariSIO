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
	inline unsigned int GetChunkLength() const;
	inline unsigned int GetCurrentPosition() const;

	bool ReadByte(uint8_t& byte);
	bool ReadWord(unsigned short& word);
	bool ReadDword(unsigned int &dword);
	bool ReadBlock(void* buf, unsigned int len);

	unsigned int CalculateCRC32() const;
	bool CalculateCRC32(unsigned int &checksum, unsigned int start_pos, unsigned int end_pos) const;

private:
	ChunkReader(RCPtr<FileIO>& f,
		unsigned int start,
		unsigned int end,
		const char* name=0);

	virtual ~ChunkReader();

	inline void SeekToCurrentPos() const;

private:
	RCPtr<FileIO> fFile;

	unsigned int fChunkStart; // absolute file position (included)
	unsigned int fChunkEnd; // absolute file position (not included)
	unsigned int fCurrentPosition; // relative position within this chunk

	char *fName;
};

inline const char* ChunkReader::GetChunkName() const
{
	return fName;
}

inline unsigned int ChunkReader::GetChunkLength() const
{
	return fChunkEnd-fChunkStart;
}

inline unsigned int ChunkReader::GetCurrentPosition() const
{
	return fCurrentPosition;
}

inline void ChunkReader::SeekToCurrentPos() const
{
	fFile->Seek(fChunkStart + fCurrentPosition);
}

#endif
