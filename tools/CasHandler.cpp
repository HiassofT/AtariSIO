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
	  fState(eInternalStatePaused),
	  fPartsIdx(0)
{
	Assert(fCasImage.IsNotNull());
	Assert(fCasImage->GetNumberOfBlocks() > 0);
	Assert(fCasImage->GetNumberOfParts() > 0);

	unsigned int total_parts = fCasImage->GetNumberOfParts();

	fPartsIdx = new unsigned int[total_parts];

	// build part index cache
	fPartsIdx[0] = 0;
	if (fCasImage->GetNumberOfParts() >= 1) {
		unsigned int i, p, part;
		part = 0;
		for (i=1; i<fCasImage->GetNumberOfBlocks(); i++) {
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

CasHandler::EState CasHandler::GetState() const
{
	switch (fState) {
	case eInternalStatePaused:
		return eStatePaused;
	case eInternalStateWaiting:
	case eInternalStatePlaying:
		return eStatePlaying;
	case eInternalStateDone:
		return eStateDone;
	}
	// we should never get here!
	AssertMsg(false, "unhandeled internal state");
	return eStateDone;
}

unsigned int CasHandler::GetCurrentPartNumber() const
{
	return fCasImage->GetBlock(fCurrentBlockNumber)->GetPartNumber();
}

unsigned int CasHandler::GetCurrentBlockBaudRate() const
{
	return fCasImage->GetBlock(fCurrentBlockNumber)->GetBaudRate();
}

unsigned int CasHandler::GetCurrentBlockGap() const
{
	return fCasImage->GetBlock(fCurrentBlockNumber)->GetGap();
}

unsigned int CasHandler::GetCurrentBlockLength() const
{
	return fCasImage->GetBlock(fCurrentBlockNumber)->GetLength();
}

bool CasHandler::SeekStart()
{
	fCurrentBlockNumber = 0;
	return true;
}

bool CasHandler::SeekEnd()
{
	fCurrentBlockNumber = GetNumberOfBlocks() - 1;
	return true;
}

bool CasHandler::SeekNextBlock(unsigned int skip)
{
	if (skip == 0) {
		Assert(false);
		return false;
	}

	if (fCurrentBlockNumber+1 < GetNumberOfBlocks()) {
		if (fCurrentBlockNumber + skip >= fCasImage->GetNumberOfBlocks()) {
			return SeekEnd();
		} else {
			fCurrentBlockNumber += skip;
			return true;
		}
	} else {
		return false;
	}
}

bool CasHandler::SeekPrevBlock(unsigned int skip)
{
	if (skip == 0) {
		Assert(false);
		return false;
	}

	if (fCurrentBlockNumber > 0) {
		if (skip > fCurrentBlockNumber) {
			return SeekStart();
		} else {
			fCurrentBlockNumber -= skip;
			return true;
		}
	} else {
		return false;
	}
}

bool CasHandler::SeekPrevPart()
{
	unsigned int part = GetCurrentPartNumber();

	if (fPartsIdx[part] == fCurrentBlockNumber) {
		if (part > 0) {
			fCurrentBlockNumber = fPartsIdx[part-1];
			return true;
		} else {
			return false;
		}
	} else {
		fCurrentBlockNumber = fPartsIdx[part];
		return true;
	}
}

bool CasHandler::SeekNextPart()
{
	unsigned int part = GetCurrentPartNumber();

	if (part+1 < GetNumberOfParts()) {
		fCurrentBlockNumber = fPartsIdx[part+1];
		return true;
	} else {
		return false;
	}
}
