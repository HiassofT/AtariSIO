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

#include <sstream>
#include <iostream>
#include <iomanip>
#include "string.h"
#include "ComBlock.h"
#include "Error.h"
#include "AtariDebug.h"

ComBlock::ComBlock(FILE*f)
	: fData(0), fFileOffset(0)
{
	unsigned char buf[4];

	int len = 0;
	if ((len = fread(buf, 1, 4, f)) != 4) {
		if (len == 0) {
			throw EOFError();
		} else {
			throw ReadError();
		}
	}

	if (buf[0] == 0xff && buf[1] == 0xff) {
		buf[0] = buf[2];
		buf[1] = buf[3];
		if (fread(buf+2, 1, 2, f) != 2) {
			throw ReadError();
		}
	}

	fStartAddress = buf[0] + (buf[1] << 8);

	unsigned int endadr = buf[2] + (buf[3] << 8);

	if (endadr < fStartAddress) {
		throw ErrorObject("invalid header in COM file");
	}

	fLen = endadr - fStartAddress + 1;

	fData = new unsigned char[fLen];

	fFileOffset = ftell(f);

	if (fread(fData, 1, fLen, f) != fLen) {
		ClearData();
		throw ReadError();
	}
}

ComBlock::ComBlock(const unsigned char* data, unsigned int len, unsigned int start_address)
	: fData(0), fFileOffset(0)
{
	if (!data) {
		Assert(false);
	}
	if (!len) {
		Assert(false);
	}
	fStartAddress = start_address;
	fLen = len;
	fData = new unsigned char[fLen];
	memcpy(fData, data, fLen);
}

ComBlock::~ComBlock()
{
	ClearData();
}

void ComBlock::ClearData()
{
	if (fData) {
		delete[] fData;
		fData = 0;
	}
}

bool ComBlock::WriteToFile(FILE* f, bool include_ffff) const
{
	unsigned char buf[4];
	if (include_ffff) {
		buf[0] = 0xff;
		buf[1] = 0xff;
		if (fwrite(buf, 1, 2, f) != 2) {
			return false;
		}
	}
	buf[0] = fStartAddress & 0xff;
	buf[1] = fStartAddress >> 8 ;
	buf[2] = (fStartAddress + fLen - 1) & 0xff;
	buf[3] = (fStartAddress + fLen - 1) >> 8;
	if (fwrite(buf, 1, 4, f) != 4) {
		return false;
	}
	if (fwrite(fData, 1, fLen, f) != fLen) {
		return false;
	}
	return true;
}

std::string ComBlock::GetDescription() const
{
	std::ostringstream s;

	s << std::hex
		<< std::setw(4) << std::setfill('0')
		<< fStartAddress
		<< '-'
		<< std::setw(4)
		<< (fStartAddress + fLen - 1)
		<< std::dec << std::setfill(' ')
		<< " (bytes: " 
		<< std::setw(5)
		<< fLen
	;
	if (fFileOffset > 0) {
		s << ", offset: "
			<< std::setw(5)
			<< fFileOffset
		;
	}
	s << ")";
	return s.str();
}
