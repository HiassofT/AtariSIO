/*
   Crc32.cpp - calculate CRC32 checksum

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

#include "Crc32.h"

static unsigned long* crc32_table=0;

static unsigned long* GetCRCTable()
{
	if (crc32_table == 0) {
		crc32_table = new unsigned long[256];
		unsigned long crc;
		for (int i=0; i<256; i++) {
			crc = i;
			for (int j=0; j<8; j++) {
			if (crc&1) {
				crc = (crc >> 1) ^ 0xedb88320L;
			} else {
				crc >>= 1;
			}
								       }
			crc32_table[i] = crc;
		}

	}
	return crc32_table;
}

unsigned long CRC32::CalcCRC32(unsigned long oldCRC, void *buf, unsigned int len)
{
	uint8_t* myBuf = (uint8_t*) buf;
	unsigned long crc = oldCRC ^ 0xffffffff;
	unsigned long* crctable = GetCRCTable();

	for (unsigned int i=0;i<len;i++) {
		crc = (crc>>8) ^ crctable[(crc^myBuf[i]) & 0xff];
	}

	crc ^= 0xffffffff;

	return crc;
}
