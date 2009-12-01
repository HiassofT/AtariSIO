#ifndef VIRTUALIMAGEOBERVER_H
#define VIRTUALIMAGEOBERVER_H

/*
   VirtualImageObserver - track write access to directory sectors

   Copyright (C) 2004 Matthias Reichl <hias@horus.com>

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

#include "RefCounted.h"
#include "AtrImage.h"

class Dos2xUtils;

class VirtualImageObserver : public RefCounted {
public:
	VirtualImageObserver(RCPtr<AtrImage> image);
	virtual ~VirtualImageObserver();

	void SetRootDirectoryObserver(RCPtr<Dos2xUtils> root);
	RCPtr<const Dos2xUtils> GetRootDirectoryObserver() const;

	void IndicateBeforeSectorWrite(unsigned int sector);
	void IndicateAfterSectorWrite(unsigned int sector);

private:
	friend class Dos2xUtils;
	void SetDirectoryObserver(unsigned int dirsec, Dos2xUtils* utils);
	void RemoveDirectoryObserver(unsigned int dirsec, Dos2xUtils* utils);

	AtrImage* fImage;

	RCPtr<Dos2xUtils> fRootDirObserver;

	Dos2xUtils** fDirectoryObserver;
	unsigned int fNumberOfSectors;

	uint8_t fOldSectorBuffer[256];
	uint8_t fNewSectorBuffer[256];
	unsigned int fBufferedSectorNumber;
};

#endif
