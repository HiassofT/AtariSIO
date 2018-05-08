#ifndef FILEIO_H
#define FILEIO_H

/*
   FileIO.h - wrapper classes for handling file IO

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
#include <sys/types.h>
#include <stdint.h>

#ifdef USE_ZLIB
#include <zlib.h>
#endif

#include "RefCounted.h"
#include "RCPtr.h"

class FileIO : public RefCounted {
public:
	FileIO();
	virtual ~FileIO();

	virtual bool OpenRead(const char* filename) = 0;
	virtual bool OpenWrite(const char* filename) = 0;

	virtual bool Close() = 0;

	virtual unsigned int ReadBlock(void* buf, unsigned int len) = 0;
	virtual unsigned int WriteBlock(const void* buf, unsigned int len) = 0;

	virtual off_t GetFileLength() = 0;
	virtual bool Seek(off_t pos) = 0;
	virtual off_t Tell() = 0;

	bool ReadByte(uint8_t& byte);
	bool WriteByte(const uint8_t& byte);

	/* read/write lo, hi byte */
	bool ReadWord(uint16_t& word);
	bool WriteWord(const uint16_t& word);

	/* read/write hi, lo byte */
	bool ReadBigEndianWord(uint16_t& word);
	bool WriteBigEndianWord(const uint16_t& word);

	/* read/write lo, mid1, mid2, hi byte */
	bool ReadDWord(uint32_t& word);
	bool WriteDWord(const uint32_t& word);

	/* read/write hi, mid2, mid1, lo byte */
	bool ReadBigEndianDWord(uint32_t& word);
	bool WriteBigEndianDWord(const uint32_t& word);

	virtual bool Unlink(const char* filename);

	virtual bool IsOpen() const = 0;
};

class StdFileIO : public FileIO
{
public:
	StdFileIO();
	virtual ~StdFileIO();

	virtual bool OpenRead(const char* filename);
	virtual bool OpenWrite(const char* filename);

	virtual bool Close();

	virtual unsigned int ReadBlock(void* buf, unsigned int len);
	virtual unsigned int WriteBlock(const void* buf, unsigned int len);

	virtual off_t GetFileLength();
	virtual bool Seek(off_t pos);
	virtual off_t Tell();

	virtual bool IsOpen() const;

private:
	typedef FileIO super;

	FILE* fFile;
};

#ifdef USE_ZLIB

class GZFileIO : public FileIO
{
public:
	GZFileIO();
	virtual ~GZFileIO();

	virtual bool OpenRead(const char* filename);
	virtual bool OpenWrite(const char* filename);

	virtual bool Close();

	virtual unsigned int ReadBlock(void* buf, unsigned int len);
	virtual unsigned int WriteBlock(const void* buf, unsigned int len);

	virtual off_t GetFileLength();
	virtual bool Seek(off_t pos);
	virtual off_t Tell();

	virtual bool IsOpen() const;

private:
	typedef FileIO super;

	gzFile fFile;
};

#endif

#endif
