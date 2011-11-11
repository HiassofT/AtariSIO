#ifndef ATRIMAGE_H
#define ATRIMAGE_H

/*
   AtrImage.h - common base class for handling ATR (Atari 8bit disk-) images

   Copyright (C) 2002-2004 Matthias Reichl <hias@horus.com>

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

#include <unistd.h>

#include "DiskImage.h"

class AtrImageConfig {
public:
	AtrImageConfig();
	~AtrImageConfig();

	AtrImageConfig(const AtrImageConfig&other);

	AtrImageConfig& operator=(const AtrImageConfig& other);
	bool operator==(const AtrImageConfig& other) const;
	bool operator!=(const AtrImageConfig& other) const;

	void DetermineDiskFormatFromLayout();
	void CalculateImageSize();

	unsigned int GetSectorLength() const;
	unsigned int GetSectorLength(unsigned int sector) const;

	EDiskFormat fDiskFormat;

	ESectorLength fSectorLength;

	unsigned int fSectorsPerTrack;
	unsigned int fTracksPerSide;
	unsigned int fSides;

	unsigned int fNumberOfSectors;
	size_t fImageSize;
};

class AtrImage : public DiskImage {
public:

	AtrImage();

	virtual ~AtrImage();

	virtual ESectorLength GetSectorLength() const { return fImageConfig.fSectorLength; }
	virtual unsigned int GetSectorLength(unsigned int sectorNumber) const { return fImageConfig.GetSectorLength(sectorNumber); }
	virtual unsigned int GetNumberOfSectors() const { return fImageConfig.fNumberOfSectors; }

	const AtrImageConfig& GetImageConfig() { return fImageConfig; }

	EDiskFormat GetDiskFormat() const { return fImageConfig.fDiskFormat; }

	unsigned int GetSectorsPerTrack() const { return fImageConfig.fSectorsPerTrack; }
	unsigned int GetTracksPerSide() const { return fImageConfig.fTracksPerSide; }
	unsigned int GetSides() const { return fImageConfig.fSides; }

	virtual size_t GetImageSize() const { return fImageConfig.fImageSize; }

	virtual bool ReadSector(unsigned int sector,
		       uint8_t* buffer,
		       unsigned int buffer_length) const;

	virtual bool WriteSector(unsigned int sector,
		       const uint8_t* buffer,
		       unsigned int buffer_length);

	virtual bool CreateImage(EDiskFormat format) = 0;
	virtual bool CreateImage(ESectorLength density, unsigned int sectors) = 0;
	virtual bool CreateImage(ESectorLength density, unsigned int sectorsPerTrack, unsigned int tracks, unsigned int sides) = 0;

	virtual bool IsAtrImage() const;
	virtual bool IsAtrMemoryImage() const;

	virtual bool ReadImageFromFile(const char* filename, bool beQuiet = false);
	virtual bool WriteImageToFile(const char* filename) const;

protected:
	bool SetFormat(EDiskFormat format);
	// note: only 1..65535 sectors are allowed
	bool SetFormat(ESectorLength density, unsigned int numberOfSectors);
	bool SetFormat(ESectorLength density, unsigned int sectors, unsigned int tracks, unsigned int sides);

	bool SetFormatFromATRHeader(const uint8_t* header);
	bool CreateATRHeaderFromFormat(uint8_t* header) const;

	ssize_t CalculateOffset(unsigned int sector) const;
	// -1 = error

private:

	void Init(); /* reset all data to zero */

	AtrImageConfig fImageConfig;
};

inline ssize_t AtrImage::CalculateOffset(unsigned int sector) const
{
	if ( (sector == 0) || (sector > fImageConfig.fNumberOfSectors) ) {
		return -1;
	}
	if (fImageConfig.fSectorLength == e256BytesPerSector) {
		if (sector <= 3) {
			return (sector-1)*128;
		} else {
			return 384 + (sector-4)*256;
		}
	} else {
		return fImageConfig.GetSectorLength(sector) * (sector - 1);
	}
}

inline AtrImageConfig::AtrImageConfig()
	: fDiskFormat(eNoDisk),
	  fSectorLength(e128BytesPerSector),
	  fSectorsPerTrack(0),
	  fTracksPerSide(0),
	  fSides(0),
	  fNumberOfSectors(0),
	  fImageSize(0)
{ }

inline AtrImageConfig::AtrImageConfig(const AtrImageConfig&other)
	: fDiskFormat(other.fDiskFormat),
	  fSectorLength(other.fSectorLength),
	  fSectorsPerTrack(other.fSectorsPerTrack),
	  fTracksPerSide(other.fTracksPerSide),
	  fSides(other.fSides),
	  fNumberOfSectors(other.fNumberOfSectors),
	  fImageSize(other.fImageSize)
{ }

inline AtrImageConfig& AtrImageConfig::operator=(const AtrImageConfig& other)
{
	fDiskFormat = other.fDiskFormat;
	fSectorLength = other.fSectorLength;
	fNumberOfSectors = other.fNumberOfSectors;
	fSectorsPerTrack = other.fSectorsPerTrack;
	fTracksPerSide = other.fTracksPerSide;
	fSides = other.fSides;
	fImageSize = other.fImageSize;
	return *this;
}

inline bool AtrImageConfig::operator==(const AtrImageConfig& other) const
{
	return
		fDiskFormat == other.fDiskFormat &&
		fSectorLength == other.fSectorLength &&
		fNumberOfSectors == other.fNumberOfSectors &&
		fSectorsPerTrack == other.fSectorsPerTrack &&
		fTracksPerSide == other.fTracksPerSide &&
		fSides == other.fSides &&
		fImageSize == other.fImageSize;
}

inline bool AtrImageConfig::operator!=(const AtrImageConfig& other) const
{
	return !(*this == other);
}

inline AtrImageConfig::~AtrImageConfig()
{ }

inline unsigned int AtrImageConfig::GetSectorLength() const
{
	switch (fSectorLength) {
	case e128BytesPerSector: return 128;
	case e256BytesPerSector: return 256;
	case e512BytesPerSector: return 512;
	case e1kPerSector: return 1024;
	case e2kPerSector: return 2048;
	case e4kPerSector: return 4096;
	case e8kPerSector: return 8192;
	default: return 0;
	}
}
inline unsigned int AtrImageConfig::GetSectorLength(unsigned int sector) const
{
	if (sector == 0 || sector > fNumberOfSectors) {
		return 0;
	}
	if (fSectorLength == e256BytesPerSector) {
		if (sector <= 3) {
			return 128;
		} else {
			return 256;
		}
	} else {
		return GetSectorLength();
	}
}

inline void AtrImageConfig::DetermineDiskFormatFromLayout()
{
	fDiskFormat = eUserDefDisk;

	if (fSectorLength == e128BytesPerSector) {
		if (fSectorsPerTrack == 18 && fTracksPerSide == 40 && fSides == 1) {
			fDiskFormat = e90kDisk;
		} else if (fSectorsPerTrack == 26 && fTracksPerSide == 40 && fSides == 1) {
			fDiskFormat = e130kDisk;
		}
	} else {
		if (fSectorsPerTrack == 18 && fTracksPerSide == 40 && fSides == 1) {
			fDiskFormat = e180kDisk;
		} else if (fSectorsPerTrack == 18 && fTracksPerSide == 40 && fSides == 2) {
			fDiskFormat = e360kDisk;
		}
	}
}

inline void AtrImageConfig::CalculateImageSize()
{
	if (fSectorLength == e256BytesPerSector) {
		fImageSize = fNumberOfSectors * 256 - 384;
	} else {
		fImageSize = fNumberOfSectors * GetSectorLength(1);
	}
}

#endif
