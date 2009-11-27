/*
   AtpTrack.cpp - representation of a track in ATP format

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
#include "AtpTrack.h"
#include "Indent.h"
#include "SIOTracer.h"
#include "AtariDebug.h"

using std::list;

AtpTrack::AtpTrack()
	: fNumberOfSectors(0),
	  fTrackNumber(0),
	  fDensity(Atari1050Model::eDensityFM)
{}

AtpTrack::~AtpTrack()
{}

void AtpTrack::AddSector(const RCPtr<AtpSector>& sec)
{
	list< RCPtr<AtpSector> >::iterator end(fSectors.end());
	list< RCPtr<AtpSector> >::iterator iter(fSectors.begin());

	while	( (iter != end) &&
		  ( (*iter)->GetPosition() <= sec->GetPosition() )
		) {
		iter++;
	}
	fSectors.insert(iter, sec);
	fNumberOfSectors++;
}

bool AtpTrack::GetSector(unsigned int id,
		RCPtr<AtpSector>& sector,
		unsigned int current_time)
{
	list< RCPtr<AtpSector> >::const_iterator end(fSectors.end());
	list< RCPtr<AtpSector> >::const_iterator current(fSectors.begin());

	sector = RCPtr<AtpSector>();
	if (fSectors.empty()) {
		return false;
	}

	// "seek" to current time position

	while	( (current != end) &&
		  ( (*current)->GetPosition() < current_time )
		) {
		current++;
	}

	// ensure that current always points to a real sector
	if (current == end) {
		current = fSectors.begin();
	}

	list< RCPtr<AtpSector> >::const_iterator iter(current);
	do {
		if ((*iter)->GetID() == id) {
			sector = *iter;
			return true;
		}
		iter++;
		if (iter == end) {
			iter = fSectors.begin();
		}
	} while (iter!=current);

	return false;
}

bool AtpTrack::InternalGetSector(unsigned int internalNumber, RCPtr<AtpSector>& sector)
{
	if (internalNumber>=fNumberOfSectors) {
		sector = RCPtr<AtpSector>();
		return false;
	} else {
		list< RCPtr<AtpSector> >::const_iterator iter(fSectors.begin());
		for (unsigned int i=0;i<internalNumber;i++) {
			iter++;
		}
		sector = *iter;
		return true;
	}
}

void AtpTrack::Dump(std::ostream& os, unsigned int indentlevel) const
{
	using std::endl;

	os << Indent(indentlevel)
	   << "begin track number "
	   << fTrackNumber
	   << " {"
	   << endl
	;
	os << Indent(indentlevel+1)
	   << "density: "
	;
	switch (fDensity) {
	case Atari1050Model::eDensityFM:
		os << "FM";
		break;
	case Atari1050Model::eDensityMFM:
		os << "MFM";
		break;
	default:
		os << "<unknown>";
		break;
	}
	os << endl;

	if (fSectors.empty()) {
		os << Indent(indentlevel+1)
		   << "empty track"
		   << endl
		;
	} else {
		os << Indent(indentlevel+1)
		   << "begin sectors {"
		   << endl
		;
		list< RCPtr<AtpSector> >::const_iterator end(fSectors.end());
		list< RCPtr<AtpSector> >::const_iterator iter(fSectors.begin());

		while (iter != end) {
			(*iter)->Dump(os, indentlevel+2);
			iter++;
		}
		os << Indent(indentlevel+1)
		   << "} // end sectors"
		   << endl
		;
	}
	os << Indent(indentlevel)
	   << "} // end track number "
	   << fTrackNumber
	   << endl
	;
}

void AtpTrack::SetTrackNumber(unsigned int trackno)
{
	fTrackNumber = trackno;
}

RCPtr<ChunkWriter> AtpTrack::BuildTrackChunk() const
{
	RCPtr<ChunkWriter> chunk(new ChunkWriter("TRAK"));

	chunk->AppendDword(fTrackNumber);
	chunk->AppendDword(fNumberOfSectors);

	switch (fDensity) {
	case Atari1050Model::eDensityFM:
		chunk->AppendDword(0);
		break;
	case Atari1050Model::eDensityMFM:
		chunk->AppendDword(1);
		break;
	default:
		std::cerr << "unknown track density!" << std::endl;
		chunk->AppendDword(0);
		break;
	}
	
	list< RCPtr<AtpSector> >::const_iterator end(fSectors.end());
	list< RCPtr<AtpSector> >::const_iterator iter(fSectors.begin());
	
	/*
	unsigned int last_position=0;
	unsigned int current_position;
	*/
	while (iter != end) {
		RCPtr<AtpSector> sec(*iter);

		RCPtr<ChunkWriter> sect_chunk = sec->BuildSectorChunk();
		chunk->AppendChunk(sect_chunk);

		iter++;
	}

	chunk->CloseChunk();

	return chunk;
}

bool AtpTrack::InitFromTRAKChunk(RCPtr<ChunkReader> chunk, bool beQuiet)
{
	fSectors.clear();
	fNumberOfSectors = 0;

	if (!chunk || strcmp(chunk->GetChunkName(),"TRAK")) {
		return false;
	}

	unsigned int trackno;
	if (!chunk->ReadDword(trackno)) return false;
	fTrackNumber = trackno;

	unsigned int secNo;
	if (!chunk->ReadDword(secNo)) return false;

	unsigned int dens;
	if (!chunk->ReadDword(dens)) return false;

	switch (dens) {
	case 0:
		fDensity = Atari1050Model::eDensityFM;
		break;
	case 1:
		fDensity = Atari1050Model::eDensityMFM;
		break;
	default:
		if (!beQuiet) {
			AERROR("unknown density %d",dens);
		}
		fDensity = Atari1050Model::eDensityFM;
	}

	// unsigned int current_position=0;
	for (uint8_t i=0;i<secNo;i++) {

		RCPtr<ChunkReader> sectorChunk;
		do {
			sectorChunk = chunk->OpenChunk();
		} while (sectorChunk && strcmp(sectorChunk->GetChunkName(),"SECT"));
		if (!sectorChunk) {
			if (!beQuiet) {
				AERROR("cannot find SECT (%d) chunk in file",i);
			}
			return false;
		}

		RCPtr<AtpSector> sector = new AtpSector;
		if (!sector->InitFromSectorChunk(sectorChunk, beQuiet)) {
			if (!beQuiet) {
				AERROR("initialization of sector %d from SECT chunk failed",i);
			}
			return false;
		}

		AddSector(sector);
	}
	return true;
}

RCPtr<ChunkWriter> AtpTrack::BuildTrackTimingChunk() const
{
	RCPtr<ChunkWriter> chunk(new ChunkWriter("TTI1"));

	chunk->AppendDword(fTrackNumber);
	chunk->AppendDword(fNumberOfSectors);

	list< RCPtr<AtpSector> >::const_iterator end(fSectors.end());
	list< RCPtr<AtpSector> >::const_iterator iter(fSectors.begin());
	
	while (iter != end) {
		RCPtr<AtpSector> sec(*iter);

		chunk->AppendDword((*iter)->GetPosition());
		chunk->AppendDword((*iter)->GetTimeLength());

		iter++;
	}

	chunk->CloseChunk();

	return chunk;
}

bool AtpTrack::SetTimingInformationFromTTI1Chunk(RCPtr<ChunkReader> chunk, bool beQuiet)
{
	if (!chunk || strcmp(chunk->GetChunkName(),"TTI1")) {
		return false;
	}

	unsigned int trackno;
	if (!chunk->ReadDword(trackno)) return false;

	if (trackno != fTrackNumber) {
		if (!beQuiet) {
			AERROR("invalid track number in timing information: expected %u got %u",
				fTrackNumber, trackno);
		}
		return false;
	}

	unsigned int secNo;
	if (!chunk->ReadDword(secNo)) return false;

	if (secNo != fNumberOfSectors) {
		if (!beQuiet) {
			AERROR("invalid number of sectors in timing info of track %u", fTrackNumber);
		}
		return false;
	}

	unsigned int last_position=0;
	bool isFirst = true;

	unsigned int current_position;
	unsigned int current_length;

	list< RCPtr<AtpSector> >::const_iterator end(fSectors.end());
	list< RCPtr<AtpSector> >::const_iterator iter(fSectors.begin());
	
	while (iter != end) {
		if (!chunk->ReadDword(current_position)) return false;

		if ( (current_position >= Atari1050Model::eDiskRotationTime) 
			||
		     ( (!isFirst) && (current_position <= last_position) ) ) {
			if (!beQuiet) {
				AERROR("illegal sector position %u", current_position);
			}
			return false;
		}

		if (!chunk->ReadDword(current_length)) return false;
		if (current_length >= Atari1050Model::eDiskRotationTime) {
			if (!beQuiet) {
				AERROR("illegal sector time length %u", current_length);
			}
			return false;
		}

		(*iter)->SetPositionAndTimeLength(current_position, current_length);
		last_position = current_position;
		isFirst = false;

		iter++;
	}
	return true;
}

