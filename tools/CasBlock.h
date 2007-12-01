#ifndef CASBLOCK_H
#define CASBLOCK_H

/*
   CasBlock - one single block of a CAS image

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

class CasBlock : public RefCounted {
public:
	CasBlock(unsigned int baudrate, unsigned int gap, unsigned int length, unsigned char* data, unsigned int partNumber = 1);

	unsigned int GetBaudRate() const;
	unsigned int GetGap() const;
	unsigned int GetLength() const;
	const unsigned char* GetData() const;

	void SetPartNumber(unsigned int part);
	unsigned int GetPartNumber() const;

protected:

	virtual ~CasBlock();

private:

	unsigned int fBaudRate;
	unsigned int fGap;
	unsigned int fLength;
	unsigned char* fData;

	unsigned int fPartNumber;
};

inline unsigned int CasBlock::GetBaudRate() const
{
	return fBaudRate;
}

inline unsigned int CasBlock::GetGap() const
{
	return fGap;
}

inline unsigned int CasBlock::GetLength() const
{
	return fLength;
}

inline const unsigned char* CasBlock::GetData() const
{
	return fData;
}

inline void CasBlock::SetPartNumber(unsigned int part)
{
	fPartNumber = part;
}

inline unsigned int CasBlock::GetPartNumber() const
{
	return fPartNumber;
}

#endif
