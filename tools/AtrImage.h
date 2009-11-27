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

	uint16_t GetSectorLength(uint16_t sector) const;

	EDiskFormat fDiskFormat;

	ESectorLength fSectorLength;

	uint16_t fSectorsPerTrack;
	uint8_t fTracksPerSide;
	uint8_t fSides;

	uint16_t fNumberOfSectors;
	uint32_t fImageSize;
};

class AtrImage : public DiskImage {
public:

	AtrImage();

	virtual ~AtrImage();

	virtual ESectorLength GetSectorLength() const { return fImageConfig.fSectorLength; }
	virtual uint16_t GetSectorLength(uint16_t sectorNumber) const { return fImageConfig.GetSectorLength(sectorNumber); }
	virtual uint16_t GetNumberOfSectors() const { return fImageConfig.fNumberOfSectors; }

	const AtrImageConfig& GetImageConfig() { return fImageConfig; }

	EDiskFormat GetDiskFormat() const { return fImageConfig.fDiskFormat; }

	uint16_t GetSectorsPerTrack() const { return fImageConfig.fSectorsPerTrack; }
	uint8_t GetTracksPerSide() const { return fImageConfig.fTracksPerSide; }
	uint8_t GetSides() const { return fImageConfig.fSides; }

	virtual size_t GetImageSize() const { return fImageConfig.fImageSize; }

	virtual bool ReadSector(uint16_t sector,
		       uint8_t* buffer,
		       size_t buffer_length) const;

	virtual bool WriteSector(uint16_t sector,
		       const uint8_t* buffer,
		       size_t buffer_length);

	virtual bool CreateImage(EDiskFormat format) = 0;
	virtual bool CreateImage(ESectorLength density, uint16_t sectors) = 0;
	virtual bool CreateImage(ESectorLength density, uint16_t sectorsPerTrack, uint8_t tracks, uint8_t sides) = 0;

	virtual bool IsAtrImage() const;
	virtual bool IsAtrMemoryImage() const;

	virtual bool ReadImageFromFile(const char* filename, bool beQuiet = false);
	virtual bool WriteImageToFile(const char* filename) const;

protected:
	bool SetFormat(EDiskFormat format);
	// note: only 1..65535 sectors are allowed
	bool SetFormat(ESectorLength density, uint32_t numberOfSectors);
	bool SetFormat(ESectorLength density, uint16_t sectors, uint8_t tracks, uint8_t sides);

	bool SetFormatFromATRHeader(const uint8_t* header);
	bool CreateATRHeaderFromFormat(uint8_t* header) const;

	ssize_t CalculateOffset(uint16_t sector) const;
	// -1 = error

private:

	void Init(); /* reset all data to zero */

	AtrImageConfig fImageConfig;
};

inline ssize_t AtrImage::CalculateOffset(uint16_t sector) const
{
	if ( (sector == 0) || (sector > fImageConfig.fNumberOfSectors) ) {
		return -1;
	}
	switch (fImageConfig.fSectorLength) {
	case e128BytesPerSector:
		return (sector-1)*128;
	case e256BytesPerSector:
		if (sector <= 3) {
			return (sector-1)*128;
		} else {
			return 384 + (sector-4)*256;
		}
	}
	return -1;
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

inline uint16_t AtrImageConfig::GetSectorLength(uint16_t sector) const
{
	if (sector == 0 || sector > fNumberOfSectors) {
		return 0;
	}
	if (fSectorLength == e256BytesPerSector && sector > 3) {
		return 256;
	} else {
		return 128;
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
	if (fSectorLength == e256BytesPerSector && fNumberOfSectors >3) {
		fImageSize = fNumberOfSectors * 256 - 384;
	} else {
		fImageSize = fNumberOfSectors * 128;
	}
}

#endif
