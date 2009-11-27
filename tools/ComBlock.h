#ifndef COMBLOCK_H
#define COMBLOCK_H

/*
   ComBlock - class representing a block of an Atari COM/EXE file

   Copyright (C) 2008 Matthias Reichl <hias@horus.com>

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

#include <string>
#include <stdio.h>

#include "RCPtr.h"
#include "RefCounted.h"
#include "AtariDebug.h"
#include "FileIO.h"

class ComBlock : public RefCounted {
public:
	// read COM block from file
	ComBlock(RCPtr<FileIO>& f);

	// create COM block from data
	ComBlock(const uint8_t* data, uint32_t len, uint16_t start_address);

	virtual ~ComBlock();

	uint16_t GetStartAddress() const;
	uint16_t GetEndAddress() const;
	uint32_t GetLength() const;
	off_t GetFileOffset() const;

	bool ContainsAddress(uint16_t) const;

	// write with COM header and, optionally, including the $FF, $FF start header
	bool WriteToFile(RCPtr<FileIO>& f, bool include_ffff = false) const;

	// write without COM header
	bool WriteRawToFile(RCPtr<FileIO>& f) const;

	uint8_t GetByte(uint16_t address) const;

	const uint8_t* GetRawData() const;

	std::string GetDescription() const;

private:
	void SetData(uint8_t* data, unsigned int len, unsigned int start_address);
	void ClearData();

	uint16_t fStartAddress;
	uint32_t fLen;
	uint8_t* fData;
	off_t fFileOffset;
};

inline uint16_t ComBlock::GetStartAddress() const
{
	return fStartAddress;
}

inline uint16_t ComBlock::GetEndAddress() const
{
	return fStartAddress + fLen - 1;
}

inline uint32_t ComBlock::GetLength() const
{
	return fLen;
}

inline off_t ComBlock::GetFileOffset() const
{
	return fFileOffset;
}

inline bool ComBlock::ContainsAddress(uint16_t adr) const
{
	return ((fStartAddress <= adr) && (fStartAddress + fLen - 1 >= adr));
}

inline uint8_t ComBlock::GetByte(uint16_t adr) const
{
	Assert(ContainsAddress(adr));
	return fData[adr - fStartAddress];
}

inline const uint8_t* ComBlock::GetRawData() const
{
	return fData;
}

#endif
