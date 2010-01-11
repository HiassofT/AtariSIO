#ifndef CASBLOCK_H
#define CASBLOCK_H

/*
   CasBlock - base class for CasBlockData and CasBlockFsk

   (c) 2007-2010 Matthias Reichl <hias@horus.com>

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

#include <stdint.h>

#include "RefCounted.h"
#include "RCPtr.h"

class CasBlock : public RefCounted {
public:
	inline unsigned int GetGap() const;
	inline unsigned int GetLength() const;

	void SetPartNumber(unsigned int part);
	unsigned int GetPartNumber() const;

	virtual bool IsDataBlock() const;
	virtual bool IsFskBlock() const;

protected:
	CasBlock(unsigned int gap, unsigned int length, unsigned int partNumber);
	virtual ~CasBlock();

	unsigned int fGap;
	unsigned int fLength;
	unsigned int fPartNumber;
};

inline unsigned int CasBlock::GetGap() const
{
	return fGap;
}

inline unsigned int CasBlock::GetLength() const
{
	return fLength;
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
