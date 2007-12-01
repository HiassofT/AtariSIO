/*
   Atari1050Model.cpp - timing (and other) specifications of the 1050

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

#include "Atari1050Model.h"

#include "AtrImage.h"
#include "AtpImage.h"

// OK, in fact sector "1" starts at position 12224, but
// I chose to store positions normalized to the start of sector 1

const unsigned int Atari1050Model::fSDSectorPositions[18] = {
	0,
	99648,
	11072,
	110720,
	22144,
	121792,
	33216,
	132864,
	44288,
	143936,
	55360,
	155008,
	66432,
	166080,
	77504,
	177152,
	88576,
	188224
};

const unsigned int Atari1050Model::fEDSectorPositions[26] = {
	0,
	99840,
	7680,
	107520,
	15360,
	115200,
	23040,
	122880,
	30720,
	130560,
	38400,
	138240,
	46080,
	145920,
	53760,
	153600,
	61440,
	161280,
	69120,
	168960,
	76800,
	176640,
	84480,
	184320,
	92160,
	192000
};

unsigned int Atari1050Model::CalculateTrackSeekTime(
	unsigned int from_track,
	unsigned int to_track)
{
	if (from_track == to_track) {
		return 0;
	}
	if (from_track < to_track) {
		// when seeking forward, the 1050 does
		// an additional half-track forward seek plus
		// a half-track backward seek. So we have
		// to add the time of one (full track) seek.

		return (to_track-from_track+1) * eTrackSeekForwardTime
			+ eHeadSettlingTime
		;
	} else { // from_track > to_track
		return (from_track-to_track) * eTrackSeekBackwardTime
			+ eHeadSettlingTime
		;
	}

}

unsigned int Atari1050Model::CalculatePositionOfSDSector(unsigned int track, unsigned int sector_id)
{
	if (sector_id < 1 || sector_id > 18 || track >= 40) {
		return 0;
	}

	// when formatting a track, the 1050 writes 215 msec long tracks
	// - although one rotation only lasts 208.3 msec.
	// since the 1050 "aligns" tracks to 210 position (and
	// waits for the next index pulse before formatting the next
	// track), we have to add 2*(215040-208333) msec per track
	return 
		( track * 2 * (e1050TrackFormatTime - eDiskRotationTime)
		  + fSDSectorPositions[sector_id-1]
		)
		% eDiskRotationTime;
}

unsigned int Atari1050Model::CalculatePositionOfEDSector(unsigned int track, unsigned int sector_id)
{
	if (sector_id < 1 || sector_id > 26 || track >= 40) {
		return 0;
	}

	// when formatting a track, the 1050 writes 215 msec long tracks
	// - although one rotation only lasts 208.3 msec.
	// since the 1050 "aligns" tracks to 210 position (and
	// waits for the next index pulse before formatting the next
	// track), we have to add 2*(215040-208333) msec per track
	return 
		( track * 2 * (e1050TrackFormatTime - eDiskRotationTime)
		  + fEDSectorPositions[sector_id-1]
		)
		% eDiskRotationTime;
}

