#ifndef MYPICODOSCODE_H
#define MYPICODOSCODE_H

/*
   MyPicoDosCode - MyPicoDos file loader

   (c) 2004 Matthias Reichl <hias@horus.com>

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

#include "DiskImage.h"

class MyPicoDosCode {
public:

	static MyPicoDosCode* GetInstance();

	inline static bool SectorNumberOK(unsigned int sec);

	bool GetMyPicoDosSector(unsigned int sector, uint8_t* buf, unsigned int buflen) const;
	bool GetBootCodeSector(unsigned int sector, uint8_t* buf, unsigned int buflen) const;

	bool WriteBootCodeToImage(RCPtr<DiskImage> img, bool autorun = false) const;

protected:

	MyPicoDosCode();

	~MyPicoDosCode();

private:

	static MyPicoDosCode* fInstance;

#include "6502/mypicodoscode.h"

public:
	static const uint8_t fCode[eCodeLength];
	static const uint8_t fBootCode[eBootLength];
};

inline MyPicoDosCode* MyPicoDosCode::GetInstance()
{
        if (fInstance == 0) {
                fInstance = new MyPicoDosCode;
        }       
        return fInstance;
}

inline bool MyPicoDosCode::SectorNumberOK(unsigned int sec)
{
	return (sec >= 1) && (sec*128 <= eCodeLength);
}
#endif
