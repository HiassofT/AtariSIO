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

class ComBlock : public RefCounted {
public:
	// read COM block from file
	ComBlock(FILE* f);

	// create COM block from data
	ComBlock(const unsigned char* data, unsigned int len, unsigned int start_address);

	virtual ~ComBlock();

	unsigned int GetStartAddress() const;
	unsigned int GetEndAddress() const;
	unsigned int GetLength() const;
	long GetFileOffset() const;

	bool ContainsAddress(unsigned int adr) const;

	bool WriteToFile(FILE* f, bool include_ffff = false) const;

	unsigned char GetByte(unsigned int address) const;

	std::string GetDescription() const;

private:
	void SetData(unsigned char* data, unsigned int len, unsigned int start_address);
	void ClearData();

	unsigned int fStartAddress;
	unsigned int fLen;
	unsigned char* fData;
	long fFileOffset;
};

inline unsigned int ComBlock::GetStartAddress() const
{
	return fStartAddress;
}

inline unsigned int ComBlock::GetEndAddress() const
{
	return fStartAddress + fLen - 1;
}

inline unsigned int ComBlock::GetLength() const
{
	return fLen;
}

inline long ComBlock::GetFileOffset() const
{
	return fFileOffset;
}

inline bool ComBlock::ContainsAddress(unsigned int adr) const
{
	return ((fStartAddress <= adr) && (fStartAddress + fLen - 1 >= adr));
}

inline unsigned char ComBlock::GetByte(unsigned int adr) const
{
	Assert(ContainsAddress(adr));
	return fData[adr - fStartAddress];
}

#endif
