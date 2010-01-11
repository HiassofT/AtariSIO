#ifndef CASDATABLOCK_H
#define CASDATABLOCK_H

/*
   CasDataBlock - standard data CAS block

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

class CasDataBlock : public CasBlock {
public:
	CasDataBlock(unsigned int gap, unsigned int length, uint8_t* data, unsigned int baudrate, unsigned int partNumber = 1);

	inline unsigned int GetBaudRate() const;
	inline const uint8_t* GetData() const;

	virtual bool IsDataBlock() const;

protected:

	virtual ~CasDataBlock();

private:
	typedef CasBlock super;

	uint8_t* fData;

	unsigned int fBaudRate;
};

inline unsigned int CasDataBlock::GetBaudRate() const
{
	return fBaudRate;
}

inline const uint8_t* CasDataBlock::GetData() const
{
	return fData;
}

#endif
