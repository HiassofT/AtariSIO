#ifndef HIGHSPEEDSIOCODE_H
#define HIGHSPEEDSIOCODE_H

/*
   HighSpeedCode.h - 6502 HighSpeedSIOCode 

   relocator copyright (c) 2002 Matthias Reichl <hias@horus.com>
   6502 code (c) ABBUC e.V. (www.abbuc.de)

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

#include <sys/types.h>
#include <stdint.h>

class HighSpeedSIOCode {
public:

	static HighSpeedSIOCode* GetInstance();

	unsigned int GetCodeSize() const;
	void RelocateCode(uint8_t* buf, unsigned short address) const;

protected:

	HighSpeedSIOCode();

	~HighSpeedSIOCode();

private:

	static HighSpeedSIOCode* fInstance;

#include "6502/atarisio-highsio.h"

	static uint8_t fSIOCode[eSIOCodeLength];
	static unsigned int fRelocatorTable[eRelocatorLength];
};

inline HighSpeedSIOCode* HighSpeedSIOCode::GetInstance()
{
        if (fInstance == 0) {
                fInstance = new HighSpeedSIOCode;
        }       
        return fInstance;
}

inline unsigned int HighSpeedSIOCode::GetCodeSize() const
{
	return eSIOCodeLength;
}

#endif
