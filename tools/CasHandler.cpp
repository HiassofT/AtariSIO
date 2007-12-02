/*
   CasHandler - Wrapper around CasImage containing current state
   and functions for tape emulation

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

#include "CasHandler.h"
#include "AtariDebug.h"
#include "FileIO.h"
#include <string.h>
#include <list>

CasHandler::CasHandler(const RCPtr<CasImage>& image, RCPtr<SIOWrapper>& siowrapper)
	: fCasImage(image),
	  fSIOWrapper(siowrapper),
	  fCurrentBaudRate(0),
	  fCurrentBlockNumber(0),
	  fState(eStatePaused),
	  fPartsIdx(0)
{
	Assert(fCasImage.IsNotNull());
	Assert(fCasImage->GetNumberOfBlocks() > 0);
	Assert(fCasImage->GetNumberOfParts() > 0);

	unsigned int total_parts = fCasImage->GetNumberOfParts();

	fPartsIdx = new unsigned int[total_parts];

	// build part index cache
	if (fCasImage->GetNumberOfParts() == 1) {
		fPartsIdx[0] = 0;
	} else {
		unsigned int i, p, part;
		part = 1;
		for (i=0; i<fCasImage->GetNumberOfBlocks(); i++) {
			if ((p = fCasImage->GetBlock(i)->GetPartNumber() != part)) {
				Assert(p < total_parts);
				fPartsIdx[p] = i;
				part = p;
			}
		}
		Assert(part + 1 == fCasImage->GetNumberOfParts());
	}
}

CasHandler::~CasHandler()
{
	if (fPartsIdx) {
		delete[] fPartsIdx;
	}
}


