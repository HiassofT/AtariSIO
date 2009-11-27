/*
   ChunkReader.cpp - simple reader for chunk format based files

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

#include "ChunkReader.h"
#include "AtariDebug.h"
#include "Crc32.h"

ChunkReader::ChunkReader(RCPtr<FileIO>& f, unsigned int start, unsigned int end, const char* name)
	: fFile(f),
	  fChunkStart(start),
	  fChunkEnd(end),
	  fCurrentPosition(0)
{
	if (name) {
		fName=new char[5];
		memcpy(fName, name, 4);
		fName[4] = 0;
	} else {
		fName = 0;
	}
}

ChunkReader::~ChunkReader()
{
	if (fName) {
		delete[] fName;
	}
}

RCPtr<ChunkReader> ChunkReader::OpenChunkFile(RCPtr<FileIO>& f)
{
	unsigned int currentPos = f->Tell();
	unsigned int endPos = f->GetFileLength();

	return new ChunkReader(f, currentPos, endPos);
}

RCPtr<ChunkReader> ChunkReader::OpenChunk()
{
	if (fChunkStart + fCurrentPosition + 8 > fChunkEnd) {
		return RCPtr<ChunkReader>();
	}

	unsigned int chunklen;
	char name[4];
	uint8_t lenbuf[4];

	SeekToCurrentPos();

	if (fFile->ReadBlock(name, 4) != 4) {
		return RCPtr<ChunkReader>();
	}
	if (fFile->ReadBlock(lenbuf, 4) != 4) {
		return RCPtr<ChunkReader>();
	}
	chunklen = lenbuf[0]
		| ( lenbuf[1] << 8)
		| ( lenbuf[2] << 16)
		| ( lenbuf[3] << 24);

	if (fChunkStart+fCurrentPosition+8+chunklen > fChunkEnd) {
		return RCPtr<ChunkReader>();
	}

	RCPtr<ChunkReader> chunk(new ChunkReader(
		fFile,
		fChunkStart+fCurrentPosition+8,
		fChunkStart+fCurrentPosition+8+chunklen,
		name)
	);
	fCurrentPosition = fCurrentPosition+8+chunklen;
	return chunk;
}

bool ChunkReader::ReadByte(uint8_t& byte)
{
	SeekToCurrentPos();

	if (fChunkStart+fCurrentPosition+1 <= fChunkEnd) {
		if (fFile->ReadBlock(&byte, 1) == 1) {
			fCurrentPosition++;
			return true;
		}
	}
	return false;
}

bool ChunkReader::ReadWord(unsigned short& word)
{
	SeekToCurrentPos();
	if (fChunkStart+fCurrentPosition+2 <= fChunkEnd) {
		uint8_t buf[2];
		if (fFile->ReadBlock(buf, 2) == 2) {
			fCurrentPosition+=2;
			word = buf[0] | (buf[1] << 8);
			return true;
		}
	}
	return false;
}

bool ChunkReader::ReadDword(unsigned int &dword)
{
	SeekToCurrentPos();
	if (fChunkStart+fCurrentPosition+4 <= fChunkEnd) {
		uint8_t buf[4];
		if (fFile->ReadBlock(buf, 4) == 4) {
			fCurrentPosition+=4;
			dword =
				  buf[0]
				| (buf[1] << 8)
				| (buf[2] << 16)
				| (buf[3] << 24);
			return true;
		}
	}
	return false;
}

bool ChunkReader::ReadBlock(void* buf, unsigned int len)
{
	SeekToCurrentPos();
	if (fChunkStart+fCurrentPosition+len <= fChunkEnd) {
		if (fFile->ReadBlock(buf, len) == len) {
			fCurrentPosition+=len;
			return true;
		}
	}
	return false;
}

unsigned int ChunkReader::CalculateCRC32() const
{
	unsigned int crc;

	if (!CalculateCRC32(crc, 0, fChunkEnd-fChunkStart)) {
		DPRINTF("internal error calculating CRC32!");
		return 0;
	}
	return crc;
}

bool ChunkReader::CalculateCRC32(unsigned int &checksum, unsigned int start_pos, unsigned int end_pos) const
{
	if (start_pos > end_pos) {
		return false;
	}
	if (fChunkStart+start_pos > fChunkEnd) {
		return false;
	}
	if (fChunkStart+end_pos > fChunkEnd) {
		return false;
	}

	unsigned long crc = CRC32::CalcCRC32(0, NULL, 0);

	unsigned int total_len = end_pos - start_pos;
	unsigned int len;

	fFile->Seek(fChunkStart + start_pos);

	uint8_t tmp_buf[1024];

	while (total_len) {
		if (total_len > 1024) {
			len = 1024;
		} else {
			len = total_len;
		}

		if (fFile->ReadBlock(tmp_buf, len) != len) {
			return false;
		}

		crc = CRC32::CalcCRC32(crc, tmp_buf, len);

		total_len -= len;
	}
	checksum = crc;
	return true;
}

