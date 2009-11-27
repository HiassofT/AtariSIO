/*
   HighSpeedCode.cpp - 6502 HighSpeedSIOCode

   relocator copyright (c) 2003, 2004 Matthias Reichl <hias@horus.com>
   6502 code (c) ABBUC e.V. (www.abbuc.de)


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

#include "HighSpeedSIOCode.h"
#include "AtariDebug.h"

HighSpeedSIOCode* HighSpeedSIOCode::fInstance = 0;

#include "6502/atarisio-highsio.c"


HighSpeedSIOCode::HighSpeedSIOCode()
{
}

HighSpeedSIOCode::~HighSpeedSIOCode()
{
}

void HighSpeedSIOCode::RelocateCode(uint8_t* buf, unsigned short address) const
{
	memcpy(buf, fSIOCode, eSIOCodeLength);

	int i;
	unsigned int adr, oldadr, newadr;
	for (i=0;i<eRelocatorLength;i++) {
		adr = fRelocatorTable[i];
		if (adr+1 > eSIOCodeLength) {
			DPRINTF("illegal address in relocator table!");
			return;
		}
		oldadr = buf[adr] + 256*buf[adr+1];
		newadr = oldadr - eOriginalAddress + address;
		// DPRINTF("relocating %04x : %04x -> %04x",adr,oldadr, newadr);
		buf[adr] = newadr & 0xff;
		buf[adr+1] = newadr >> 8;
	}
}

