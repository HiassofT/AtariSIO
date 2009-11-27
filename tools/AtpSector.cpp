/*
   AtpSector.cpp - data of an sector according to the ATP format

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

#include <string.h>

#include "AtpSector.h"
#include "Indent.h"
#include "Atari1050Model.h"
#include "AtariDebug.h"

AtpSector::AtpSector(
		unsigned int id,
		unsigned int data_len,
		const uint8_t* data,
	       	unsigned int pos,
		unsigned int time_len,
		uint8_t status)
	: fID(id),
	  fDataLength(data_len),
	  fPosition(pos),
	  fTimeLength(time_len),
	  fSectorStatus(status)
{
	if (fDataLength) {
		fSectorData = new uint8_t[fDataLength];
		memcpy(fSectorData, data, fDataLength);
	} else {
		fSectorData = 0;
	}
}

AtpSector::AtpSector()
	: fID(0),
	  fDataLength(0),
	  fSectorData(0),
	  fPosition(0),
	  fTimeLength(0),
	  fSectorStatus(255)
{
}

AtpSector::~AtpSector()
{
	if (fSectorData) {
		delete[] fSectorData;
	}
	fSectorData = 0;
	fDataLength = 0;
}

bool AtpSector::GetData(uint8_t* dest, unsigned int len) const
{
	if (len != fDataLength) {
		return false;
	} else {
		memcpy(dest, fSectorData, len);
		return true;
	}
}

bool AtpSector::SetData(const uint8_t* src, unsigned int len)
{
	if (len != fDataLength) {
		return false;
	} else {
		memcpy(fSectorData, src, len);
		return true;
	}
}

void AtpSector::Dump(std::ostream& os, unsigned int indentlevel) const
{
	using std::endl;
	os << Indent(indentlevel)
	   << "id: "
	   << fID
	   << " datalen: "
	   << fDataLength
	   << " pos: "
	   << fPosition
	   << " timelen: "
	   << fTimeLength
	   << " status: "
	;
	os.setf(std::ios::hex, std::ios::basefield);
	os.setf(std::ios::showbase);
	os
	   << (unsigned int)fSectorStatus
	   << endl
	;
	os.setf(std::ios::dec, std::ios::basefield);
}

RCPtr<ChunkWriter> AtpSector::BuildSectorChunk() const
{
	RCPtr<ChunkWriter> chunk(new ChunkWriter("SECT"));

	chunk->AppendDword(fID);
	chunk->AppendDword(fDataLength);
	chunk->AppendByte(fSectorStatus);
	chunk->AppendBlock(fSectorData, fDataLength);

	chunk->CloseChunk();

	return chunk;
}

bool AtpSector::InitFromSectorChunk(RCPtr<ChunkReader> chunk, bool beQuiet)
{
	if (fSectorData) {
		delete[] fSectorData;
		fSectorData = 0;
		fDataLength = 0;
	}

	if (!chunk->ReadDword(fID)) return false;
	if (fID < 1 || fID > 26) {
		if (!beQuiet) {
			AERROR("illegal sector ID %u", fID);
		}
		return false;
	}
	if (!chunk->ReadDword(fDataLength)) return false;
	if (fDataLength != 128) {
		if (!beQuiet) {
			AERROR("illegal sector data length %u", fDataLength);
		}
		return false;
	}
	if (!chunk->ReadByte(fSectorStatus)) return false;

	try {
		fSectorData = new uint8_t[fDataLength];
	}
	catch(...) {
		fSectorData = 0;
		if (!beQuiet) {
			AERROR("allocating sector data failed");
		}
		return false;
	}
	if (!chunk->ReadBlock(fSectorData, fDataLength)) {
		delete[] fSectorData;
		fSectorData = 0;
		return false;
	}
	return true;
}
