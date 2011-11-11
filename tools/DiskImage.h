#ifndef DISKIMAGE_H
#define DISKIMAGE_H

/*
   DiskImage.h - common base class for handling images

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

#include <unistd.h>
#include <stdint.h>

#include "RefCounted.h"
#include "RCPtr.h"


typedef enum { eNoDisk=0, e90kDisk=1, e130kDisk=2, e180kDisk=3, e360kDisk=4, eUserDefDisk=5} EDiskFormat;
typedef enum {
	e128BytesPerSector=128,
	e256BytesPerSector=256,
	e512BytesPerSector=512,
	e1kPerSector=1024,
	e2kPerSector=2048,
	e4kPerSector=4096,
	e8kPerSector=8192
} ESectorLength;

inline ESectorLength SectorLength(bool isDD) { if (isDD) { return e256BytesPerSector; } else { return e128BytesPerSector; } }

class DiskImage : public RefCounted {
public:

	DiskImage();

	virtual ~DiskImage();

	const char* GetFilename() const;
	void SetFilename(const char*filename);

	inline void SetWriteProtect(bool on);
	inline bool IsWriteProtected() const;

	virtual ESectorLength GetSectorLength() const = 0;
	virtual unsigned int GetNumberOfSectors() const = 0;
	virtual size_t GetImageSize() const = 0;

	virtual bool ReadImageFromFile(const char* filename, bool beQuiet = false) = 0;
	virtual bool WriteImageToFile(const char* filename) const = 0;
	bool WriteBackImageToFile() const;

	virtual bool IsAtrImage() const;
	virtual bool IsAtpImage() const;

	inline void SetIsVirtualImage(bool on);
	inline bool IsVirtualImage() const;

	inline bool Changed() const;

	inline void SetChanged(bool) const;

	virtual bool ReadSector(unsigned int sector,
		uint8_t* buffer,	
		unsigned int buffer_length) const = 0;

	virtual bool WriteSector(unsigned int sector,
		const uint8_t* buffer,	
		unsigned int buffer_length) = 0;

private:

	char* fFilename;
	bool fWriteProtect;
	mutable bool fChanged;
	bool fIsVirtualImage;
};

inline bool DiskImage::Changed() const
{
	return fChanged;
}

inline void DiskImage::SetChanged(bool changed) const
{
	fChanged = changed;
}

inline void DiskImage::SetWriteProtect(bool on)
{
	fWriteProtect = on;
}

inline bool DiskImage::IsWriteProtected() const
{
	return fWriteProtect;
}

inline void DiskImage::SetIsVirtualImage(bool on)
{
	fIsVirtualImage = on;
}

inline bool DiskImage::IsVirtualImage() const
{
	return fIsVirtualImage;
}

#endif
