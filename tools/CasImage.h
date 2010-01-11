#ifndef CASIMAGE_H
#define CASIMAGE_H

/*
   CasImage - CAS Image class

   (c) 2007 Matthias Reichl <hias@horus.com>

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
#include "RCPtr.h"
#include "CasBlock.h"

#include "FileIO.h"

class CasImage : public RefCounted {
public:
	CasImage();

	bool ReadImageFromFile(const char* filename);

	unsigned int GetNumberOfBlocks() const;
	unsigned int GetNumberOfParts() const;

	const char* GetDescription() const;
	const char* GetFilename() const;

	RCPtr<CasBlock> GetBlock(unsigned int blockno);

protected:

	virtual ~CasImage();

private:
	void FreeData();
	void AllocTempBuf();
	void FreeTempBuf();

	void SetDescription(uint8_t* str, unsigned int length);

	enum EBlockType {
		eBlockIOError = 0,
		eBlockEOF = 1,
		eUnknownBlock = 2,
		eFujiBlock = 3,
		eBaudBlock = 4,
		eDataBlock = 5,
		eFskBlock = 6
	};

	EBlockType ReadBlockFromFile(
		RCPtr<FileIO> fileio,
		unsigned int& length,
		unsigned int& aux,
		bool readUnknownBlockData = true);

	unsigned int fNumberOfBlocks;
	unsigned int fNumberOfParts;

	RCPtr<CasBlock>* fCasBlocks;

	char* fDescription;
	char* fFilename;

	uint8_t* fTempBuf;
};

inline unsigned int CasImage::GetNumberOfBlocks() const
{
	return fNumberOfBlocks;
}

inline unsigned int CasImage::GetNumberOfParts() const
{
	return fNumberOfParts;
}

inline const char* CasImage::GetDescription() const
{
	return fDescription;
}

inline const char* CasImage::GetFilename() const
{
	return fFilename;
}

#endif
