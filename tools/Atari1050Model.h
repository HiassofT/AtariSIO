#ifndef ATARI1050MODEL_H
#define ATARI1050MODEL_H

/*
   Atari1050Model.h - timing (and other) specifications of the 1050

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

#include <sys/types.h>
#include "RCPtr.h"

class AtrImage;
class AtpImage;

namespace Atari1050Model {

	// density of a disk/track
	enum EDiskDensity {
		eDensityFM = 0,
		eDensityMFM = 1
	};

	// time for one disk rotation, in usec
	enum { eDiskRotationTime = 208333 };

	// seek time from one track to the next, in usec
	enum { eTrackSeekForwardTime = 20316 };

	// seek time from one track to the previous, in usec
	enum { eTrackSeekBackwardTime = 20292 };

	// head settling time (after track seeks), in usec
	enum { eHeadSettlingTime = 20154 };

	// when formatting a track, the 1050 thinks it will be
	// 215 msec long (it initializes the 6532 counter with
	// a value of $D2)
	enum { e1050TrackFormatTime = 215040 };

	// time until the drive motor has reached the
	// nominal 288RPMs, in usec
	enum { eDiskSpinUpTime = 500000 };
		 
	// time the motor will be running after the last command
	enum { eMotorOffDelay = 8000000 };
		 
	// time to read a single density sector from disk
	// (including header, data, and CRC) from disk
	enum { eSDSectorTimeLength = 9920 };

	// time to read a single density sector from disk
	// (including header, data, and CRC) from disk
	enum { eEDSectorTimeLength = 5600 };

	// time the drive spends to find a sector (eg on read
	// errors) until the drive gives up
	// set to 15 disk rotations (approx. 3 seconds)
	enum { eSectorRetryTime = 3125000 };


	// starting positions of the sectors in standard SD format
	extern const unsigned int fSDSectorPositions[18];
	
	// starting positions of the sectors in standard ED format
	extern const unsigned int fEDSectorPositions[26];
	

	// calculate the time the 1050 needs to seek from track
	// "from_track" to track "to_track"
	unsigned int CalculateTrackSeekTime(unsigned int from_track, unsigned int to_track);

	// calculate the position of the given sector relative to
	// sector "1" in standard 1050 SD format
	unsigned int CalculatePositionOfSDSector(unsigned int track, unsigned int sector_id);

	// calculate the position of the given sector relative to
	// sector "1" in standard 1050 ED format
	unsigned int CalculatePositionOfEDSector(unsigned int track, unsigned int sector_id);

};

#endif

