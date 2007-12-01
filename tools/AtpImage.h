#ifndef ATPIMAGE_H
#define ATPIMAGE_H

/*
   AtpImage.h - implementation of the ATP image format

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

#include "DiskImage.h"

#include "AtpTrack.h"
#include "Atari1050Model.h"

class AtpImage : public DiskImage {
public:
	AtpImage(unsigned int numberOfTracks=40);
	virtual ~AtpImage();

	// set density for of all tracks
	bool SetDensity(Atari1050Model::EDiskDensity dens);

	// set density of given track
	bool SetDensity(Atari1050Model::EDiskDensity dens, unsigned int trackno);

	// get density of a track
	Atari1050Model::EDiskDensity GetDensity(unsigned int trackno = 0) const;

	// delete previous data and allocate 'tracks' empty tracks.
	void SetNumberOfTracks(unsigned int tracks);

	// add an AtpSector to the specified track.
	// internally the sectors are sorted by their (absolute)
	// position value
	bool AddSector(unsigned int trackno, const RCPtr<AtpSector>& sector);

	// try to find a sector with specified ID value starting
	// from (and including!) the current position on the
	// given track number.
	// On success "true" is returned and the sector parameter
	// is set.
	// On failure (when no sector with the given ID exists
	// in this track), false is returned.
	bool GetSector(unsigned int trackno,
			unsigned int sectorID,
			RCPtr<AtpSector>& sector,
			unsigned int current_time = 0) const;

	// DiskImage methods

	virtual inline ESectorLength GetSectorLength() const;
	virtual unsigned int GetNumberOfSectors() const;
	virtual unsigned int GetImageSize() const;

	virtual bool ReadSector(unsigned int sector,
		unsigned char* buffer,
		unsigned int buffer_length) const;

	virtual bool WriteSector(unsigned int sector,
		const unsigned char* buffer,
		unsigned int buffer_length);

	// dump internal information
	void Dump(std::ostream& os, unsigned int indentlevel=0);

	virtual bool ReadImageFromFile(const char* filename, bool beQuiet = false);
	virtual bool WriteImageToFile(const char* filename) const;

	virtual bool IsAtpImage() const;

	// blank init all sector to standard SD/ED format
	bool InitBlankSD();
	bool InitBlankED();

private:
	void AllocData();
	void FreeData();

	RCPtr<ChunkWriter> BuildHeaderChunk() const;

	bool InitFromHeaderChunk(RCPtr<ChunkReader> chunk);

private:
	AtpTrack* fTracks;
	unsigned int fNumberOfTracks;

};

inline ESectorLength AtpImage::GetSectorLength() const
{
	return e128BytesPerSector;
}

#endif