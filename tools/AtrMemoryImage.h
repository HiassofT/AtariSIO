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
	virtual bool CreateImage(ESectorLength density, unsigned int sectors);
	virtual bool CreateImage(ESectorLength density, unsigned int sectorsPerTrack, unsigned int tracks, unsigned int sides);

	void FreeImageData();

	virtual bool ReadImageFromFile(const char* filename, bool beQuiet = false);
	virtual bool WriteImageToFile(const char* filename) const;

	bool ReadSector(unsigned int sector,
		       uint8_t* buffer,
		       unsigned int buffer_length) const;

	bool WriteSector(unsigned int sector,
		       const uint8_t* buffer,
		       unsigned int buffer_length);

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

	uint8_t CalculateDiSectorChecksum(uint8_t* buf, unsigned int len) const;

	bool SetSectorInUse(unsigned int sector, bool inUse);

	typedef AtrImage super;

	uint8_t *fData;

};

#endif
