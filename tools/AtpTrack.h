#ifndef ATPTRACK_H
#define ATPTRACK_H

/*
   AtpTrack.h - representation of a track in ATP format

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

#include <list>

#include "AtpSector.h"
#include "Atari1050Model.h"
#include "ChunkWriter.h"
#include "ChunkReader.h"

class AtpTrack {
public:
	AtpTrack();
	~AtpTrack();

	// set density of this track
	inline void SetDensity(Atari1050Model::EDiskDensity dens);

	// get density of this track
	inline Atari1050Model::EDiskDensity GetDensity() const;

	// add an AtpSector to the track
	// internally the sectors are sorted by their (absolute)
	// position value
	void AddSector(const RCPtr<AtpSector>& sec);

	// try to find a sector with specified ID value starting
	// from (and including!) the current position.
	// On success "true" is returned and the sector parameter
	// is set.
	// On failure (when no sector with the given ID exists
	// in this track), false is returned.
	bool GetSector(unsigned int id,
			RCPtr<AtpSector>& sector,
			unsigned int current_time = 0);

	
	// create ATP "TRAK" chunk from internal data
	RCPtr<ChunkWriter> BuildTrackChunk() const;

	// create ATP "TTI1" chunk from internal data
	RCPtr<ChunkWriter> BuildTrackTimingChunk() const;

	// set internal data from ATP "TRAK" chunk
	bool InitFromTRAKChunk(RCPtr<ChunkReader> chunk, bool beQuiet);

	// dumps internal structure to stream
	void Dump(std::ostream& os, unsigned int indentlevel=0) const;

	// get total number of sectors in this track
	inline unsigned int GetNumberOfSectors() const;

	// get the 'internalNumber'th sector of this track.
	// valid values for 'internalNumber' are 0 to (GetNumberOfSectors()-1)
	// If this parameter is out of range, this method returns false
	bool InternalGetSector(unsigned int internalNumber, RCPtr<AtpSector>& sector);

private:
	friend class AtpImage;
	void SetTrackNumber(unsigned int trackno);
	inline unsigned int GetTrackNumber() const;

	bool SetTimingInformationFromTTI1Chunk(RCPtr<ChunkReader> chunk, bool beQuiet);

private:
	std::list< RCPtr<AtpSector> > fSectors;
	unsigned int fNumberOfSectors;
	unsigned int fTrackNumber;
	Atari1050Model::EDiskDensity fDensity;
};

inline void AtpTrack::SetDensity(Atari1050Model::EDiskDensity dens)
{
	fDensity = dens;
}

inline Atari1050Model::EDiskDensity AtpTrack::GetDensity() const
{
	return fDensity;
}

inline unsigned int AtpTrack::GetNumberOfSectors() const
{
	return fNumberOfSectors;
}

inline unsigned int AtpTrack::GetTrackNumber() const
{
	return fTrackNumber;
}

#endif
