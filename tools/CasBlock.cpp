/*
   CasBlock - one single block of a CAS image

   (c) 2007 Matthias Reichl <hias@horus.com>

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

#include "CasBlock.h"
#include <string.h>

CasBlock::CasBlock(
	unsigned int baudrate,
	unsigned int gap,
	unsigned int length,
	unsigned char* data,
	unsigned int partnumber)
	: fBaudRate(baudrate),
	  fGap(gap),
	  fLength(length),
	  fPartNumber(partnumber)
{
	if (length) {
		fData = new unsigned char[length];
		memcpy(fData, data, length);
	} else {
		fData = 0;
	}
}

CasBlock::~CasBlock()
{
	if (fData) {
		delete[] fData;
	}
}
