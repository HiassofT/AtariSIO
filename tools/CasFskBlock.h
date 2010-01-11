#ifndef CASFSKBLOCK_H
#define CASFSKBLOCK_H

/*
   CasFskBlock - extended fsk CAS block

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

#include "CasBlock.h"

class CasFskBlock : public CasBlock {
public:
	CasFskBlock(unsigned int gap, unsigned int byte_length, uint8_t* data, unsigned int partNumber = 1);

	inline const uint16_t* GetFskData() const;

	virtual bool IsFskBlock() const;

protected:

	virtual ~CasFskBlock();

private:
	typedef CasBlock super;

	uint16_t* fFskData;

	unsigned int fPartNumber;
};

inline const uint16_t* CasFskBlock::GetFskData() const
{
	return fFskData;
}

#endif
