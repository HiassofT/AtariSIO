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

#include <iostream>
#include <iomanip>
#include "string.h"
#include "ComBlock.h"
#include "Error.h"
#include "AtariDebug.h"

class ReadErrorLen : public ErrorObject {
public:
	ReadErrorLen(unsigned int total, unsigned int len)
	{
		char tmpstr[80];
		snprintf(tmpstr, 80, "read error: got %d of %d bytes",
			len, total);
		SetDescription(tmpstr);
	}
	virtual ~ReadErrorLen() {}
};


ComBlock::ComBlock(RCPtr<FileIO>& f)
	: fData(0), fFileOffset(0)
{
	uint16_t endadr;
	unsigned int blen;
	Assert(f.IsNotNull());
	Assert(f->IsOpen());

	if (!f->ReadWord(fStartAddress)) {
		throw EOFError();
	}
	while (fStartAddress == 0xffff) {
		if (!f->ReadWord(fStartAddress)) {
			throw ReadError();
		}
	}
	if (!f->ReadWord(endadr)) {
		throw ReadError();
	}

	if (endadr < fStartAddress) {
		throw ErrorObject("invalid header in COM file");
	}

	fLen = endadr - fStartAddress + 1;

	fData = new uint8_t[fLen];

	fFileOffset = f->Tell();

	if ((blen = f->ReadBlock(fData, fLen)) != fLen) {
		ClearData();
		throw ReadErrorLen(fLen, blen);
	}
}

ComBlock::ComBlock(const uint8_t* data, unsigned int len, unsigned int start_address)
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
	fData = new uint8_t[fLen];
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

bool ComBlock::WriteToFile(RCPtr<FileIO>& f, bool include_ffff) const
{
	uint16_t tmp;
	Assert(f.IsNotNull());
	Assert(f->IsOpen());

	if (include_ffff) {
		tmp = 0xffff;
		if (!f->WriteWord(tmp)) {
			return false;
		}
	}
	if (!f->WriteWord(fStartAddress)) {
		return false;
	}
	tmp = fStartAddress + fLen - 1;
	if (!f->WriteWord(tmp)) {
		return false;
	}

	return WriteRawToFile(f);
}

bool ComBlock::WriteRawToFile(RCPtr<FileIO>& f) const
{
	Assert(f.IsNotNull());
	Assert(f->IsOpen());
	if (f->WriteBlock(fData, fLen) != fLen) {
		return false;
	}
	return true;
}

std::string ComBlock::GetDescription(bool offsetInDecimal) const
{
	char tmpstr[80];

	if (offsetInDecimal) {
		snprintf(tmpstr, 80, "%04x-%04x (bytes: %5d, offset: %6ld)",
			fStartAddress, fStartAddress + fLen - 1,
			fLen, (long) fFileOffset);
	} else {
		snprintf(tmpstr, 80, "%04x-%04x (bytes: %4x, offset: %6lx)",
			fStartAddress, fStartAddress + fLen - 1,
			fLen, (long) fFileOffset);
	}
	return tmpstr;
}
