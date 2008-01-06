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

#include <sys/types.h>
#include <sys/time.h>

#include "RefCounted.h"
#include "RCPtr.h"
#include "CasImage.h"
#include "SIOWrapper.h"
#include "SIOTracer.h"
#include "MiscUtils.h"

class CasHandler : public RefCounted {
public:
	CasHandler(const RCPtr<CasImage>& image, RCPtr<SIOWrapper>& siowrapper);

	unsigned int GetCurrentBlockNumber() const;
	unsigned int GetCurrentPartNumber() const;
	unsigned int GetCurrentBlockBaudRate() const;
	unsigned int GetCurrentBlockGap() const;
	unsigned int GetCurrentBlockLength() const;

	unsigned int GetNumberOfBlocks() const;
	unsigned int GetNumberOfParts() const;

	const char* GetFilename() const;
	const char* GetDescription() const;

	// external state information
	enum EState {
		eStatePaused,
		eStatePlaying,
		eStateDone
	};

	EState GetState() const;

	void SetPause(bool on);

	bool SeekStart();
	bool SeekEnd();
	bool SeekNextBlock(unsigned int skip=1);
	bool SeekPrevBlock(unsigned int skip=1);
	bool SeekNextPart();
	bool SeekPrevPart();

	enum EPlayResult {
		eGotKeypress,
		eGotSignal
	};

	EPlayResult DoPlaying();

protected:

	virtual ~CasHandler();

private:

	void SetupNextBlock();

	// returns false if already past start time
	bool CalculateWaitTime(struct timeval& tv);

	enum EInternalState {
		eInternalStatePaused,
		eInternalStateWaiting,
		eInternalStatePlaying,
		eInternalStateDone
	};

	RCPtr<CasImage> fCasImage;
	RCPtr<SIOWrapper> fSIOWrapper;
	unsigned int fCurrentBaudRate;
	unsigned int fCurrentBlockNumber;

	EInternalState fState;

	unsigned int* fPartsIdx;

	MiscUtils::TimestampType fBlockStartTime;

	SIOTracer* fTracer;
};

inline unsigned int CasHandler::GetCurrentBlockNumber() const
{
	return fCurrentBlockNumber;
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

#endif
