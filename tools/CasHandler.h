#ifndef CASHANDLER_H
#define CASHANDLER_H

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

#include "RefCounted.h"
#include "RCPtr.h"
#include "CasImage.h"
#include "SIOWrapper.h"

class CasHandler : public RefCounted {
public:
	CasHandler(const RCPtr<CasImage>& image, RCPtr<SIOWrapper>& siowrapper);

	unsigned int GetCurrentBaudRate() const;
	unsigned int GetCurrentBlockNumber() const;
	unsigned int GetCurrentPartNumber() const;

	unsigned int GetNumberOfBlocks() const;
	unsigned int GetNumberOfParts() const;

	const char* GetFilename() const;
	const char* GetDescription() const;

	enum EState {
		eStatePaused,
		eStateRunning,
		eStateDone
	};

	void SetState(EState state);
	EState GetState() const;

protected:

	virtual ~CasHandler();

private:
	RCPtr<CasImage> fCasImage;
	RCPtr<SIOWrapper> fSIOWrapper;
	unsigned int fCurrentBaudRate;
	unsigned int fCurrentBlockNumber;

	EState fState;

	unsigned int* fPartsIdx;
};

inline unsigned int CasHandler::GetCurrentBaudRate() const
{
	return fCurrentBaudRate;
}

inline unsigned int CasHandler::GetCurrentBlockNumber() const
{
	return fCurrentBaudRate;
}

inline unsigned int CasHandler::GetNumberOfBlocks() const
{
	return fCasImage->GetNumberOfBlocks();
}

inline unsigned int CasHandler::GetNumberOfParts() const
{
	return fCasImage->GetNumberOfParts();
}

inline const char* CasHandler::GetDescription() const
{
	return fCasImage->GetDescription();
}

inline const char* CasHandler::GetFilename() const
{
	return fCasImage->GetFilename();
}

inline void CasHandler::SetState(CasHandler::EState state)
{
	fState = state;
}

inline CasHandler::EState CasHandler::GetState() const
{
	return fState;
}

#endif
