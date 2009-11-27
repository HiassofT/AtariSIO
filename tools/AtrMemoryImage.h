#ifndef ATRMEMORYIMAGE_H
#define ATRMEMORYIMAGE_H

/*
   AtrMemoryImage.h - access ATR images in RAM.

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

#include "AtrImage.h"

class AtrMemoryImage : public AtrImage {
public:

	AtrMemoryImage();

	virtual ~AtrMemoryImage();

	// DiskImage methods
	virtual bool CreateImage(EDiskFormat format);
	virtual bool CreateImage(ESectorLength density, uint16_t sectors);
	virtual bool CreateImage(ESectorLength density, uint16_t sectorsPerTrack, uint8_t tracks, uint8_t sides);

	void FreeImageData();

	virtual bool ReadImageFromFile(const char* filename, bool beQuiet = false);
	virtual bool WriteImageToFile(const char* filename) const;

	bool ReadSector(uint16_t sector,
		       uint8_t* buffer,
		       size_t buffer_length) const;

	bool WriteSector(uint16_t sector,
		       const uint8_t* buffer,
		       size_t buffer_length);

	virtual bool IsAtrMemoryImage() const;
	virtual void SetWriteProtect(bool on);

	friend class DCMCodec;

protected:

private:

	enum EImageType {
		eAtrImageType = 0,
		eAtrGzImageType = 1,
		eXfdImageType = 2,
		eXfdGzImageType = 3,
		eDcmImageType = 4,
		eDcmGzImageType = 5,
		eDiImageType = 6,
		eDiGzImageType = 7,
		eUnknownImageType=99
	};

	EImageType DetermineImageTypeFromFilename(const char* filename) const;

	bool ReadImageFromAtrFile(const char* filename, bool beQuiet);
	bool WriteImageToAtrFile(const char* filename, const bool useGz) const;

	bool ReadImageFromXfdFile(const char* filename, bool beQuiet);
	bool WriteImageToXfdFile(const char* filename, const bool useGz) const;

	bool ReadImageFromDcmFile(const char* filename, bool beQuiet);
	bool WriteImageToDcmFile(const char* filename, const bool useGz) const;

	bool ReadImageFromDiFile(const char* filename, bool beQuiet);
	bool WriteImageToDiFile(const char* filename, const bool useGz) const;

	uint8_t CalculateDiSectorChecksum(uint8_t* buf, size_t len) const;

	bool SetSectorInUse(uint16_t sector, bool inUse);

	typedef AtrImage super;

	uint8_t *fData;

};

#endif
