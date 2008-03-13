/*
   AtariComMemory - helper class for merging COM file blocks

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

#include <string.h>
#include "AtariComMemory.h"

AtariComMemory::AtariComMemory()
{
	Clear();
}

AtariComMemory::~AtariComMemory()
{
}

void AtariComMemory::Clear()
{
	fContainsData = false;
	fMinAddress = eMemSize - 1;
	fMaxAddress = 0;
	memset(fData, 0, eMemSize);
}

bool AtariComMemory::WriteComBlockToMemory(const RCPtr<ComBlock>& blk)
{
	unsigned int start = blk->GetStartAddress();
	unsigned int end = blk->GetEndAddress();
	Assert(end < eMemSize);

	unsigned int a;
	for (a = start; a <= end; a++) {
		fData[a] = blk->GetByte(a);
	}
	if (fContainsData) {
		if (start < fMinAddress) {
			fMinAddress = start;
		}
		if (end > fMaxAddress) {
			fMaxAddress = end;
		}
	} else {
		fContainsData = true;
		fMinAddress = start;
		fMaxAddress = end;
	}
	return true;
}

RCPtr<ComBlock> AtariComMemory::AsComBlock() const
{
	Assert(ContainsData());
	unsigned int len = fMaxAddress - fMinAddress + 1;
	return new ComBlock(fData + fMinAddress, len, fMinAddress);
}
