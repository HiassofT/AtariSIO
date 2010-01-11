/*
   CasFskBlock - standard data CAS block

   (c) 2007-2010 Matthias Reichl <hias@horus.com>

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

#include "CasFskBlock.h"
#include <string.h>

CasFskBlock::CasFskBlock(
	unsigned int gap,
	unsigned int length,
	uint8_t* data,
	unsigned int partnumber)
	: super(gap, length/2, partnumber)
{
	if (fLength) {
		unsigned int i;
		fFskData = new uint16_t[fLength];
		for (i=0; i<fLength; i++) {
			fFskData[i] = data[i*2] | (data[i*2+1] << 8);
		}
	} else {
		fFskData = 0;
	}
}

CasFskBlock::~CasFskBlock()
{
	if (fFskData) {
		delete[] fFskData;
	}
}

bool CasFskBlock::IsFskBlock() const
{
	return true;
}
