/*
   CasHandler - Wrapper around CasImage containing current state
   and functions for tape emulation

   (c) 2007-2008 Matthias Reichl <hias@horus.com>

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
#include <signal.h>

#define MAX_KERNEL_BLOCK_SIZE 32

CasHandler::CasHandler(const RCPtr<CasImage>& image, RCPtr<SIOWrapper>& siowrapper)
	: fCasImage(image),
	  fSIOWrapper(siowrapper),
	  fCurrentBaudRate(0),
	  fCurrentBlockNumber(0),
	  fState(eInternalStatePaused),
	  fUnpauseState(eInternalStateWaiting),
	  fPartsIdx(0)
{
	Assert(fCasImage.IsNotNull());
	Assert(fCasImage->GetNumberOfBlocks() > 0);
	Assert(fCasImage->GetNumberOfParts() > 0);

	fTracer = SIOTracer::GetInstance();

	unsigned int total_parts = fCasImage->GetNumberOfParts();

	fPartsIdx = new unsigned int[total_parts];

	// build part index cache
	fPartsIdx[0] = 0;
	if (fCasImage->GetNumberOfParts() >= 1) {
		unsigned int i, p, part;
		part = 0;
		for (i=1; i<fCasImage->GetNumberOfBlocks(); i++) {
			if ((p = fCasImage->GetBlock(i)->GetPartNumber()) != part) {
				//TRACE("part(%d) = %d", i, p);
				Assert(p < total_parts);
				fPartsIdx[p] = i;
				part = p;
			} else {
				//TRACE("part(%d) = %d", i, p);
			}

		}
		Assert(part + 1 == fCasImage->GetNumberOfParts());
	}
	SetupNewBlock();
}

CasHandler::~CasHandler()
{
	if (fState == eInternalStatePlaying) {
		AWARN("~CasHandler: still in playing state");
		fSIOWrapper->EndTapeBlock();
	}
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
	case eInternalStateStartPlaying:
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
	return fCurrentCasBlock->GetPartNumber();
}

unsigned int CasHandler::GetCurrentBlockBaudRate() const
{
	return fCurrentCasBlock->GetBaudRate();
}

unsigned int CasHandler::GetCurrentBlockGap() const
{
	return fCurrentCasBlock->GetGap();
}

unsigned int CasHandler::GetCurrentBlockLength() const
{
	return fCurrentCasBlock->GetLength();
}

void CasHandler::SetupNewBlock()
{
	fCurrentCasBlock = fCasImage->GetBlock(fCurrentBlockNumber);
	fCurrentBytePos = 0;
	fBlockStartTime = MiscUtils::GetCurrentTime() + GetCurrentBlockGap() * 1000;
	fSIOWrapper->SetTapeBaudrate(GetCurrentBlockBaudRate() * fTapeSpeedPercent / 100);
}


bool CasHandler::SeekStart()
{
	fCurrentBlockNumber = 0;
	SetupNewBlock();
	fTracer->IndicateCasBlockChanged();
	return true;
}

bool CasHandler::SeekEnd()
{
	fCurrentBlockNumber = GetNumberOfBlocks() - 1;
	SetupNewBlock();
	fTracer->IndicateCasBlockChanged();
	return true;
}

bool CasHandler::SeekNextBlock(unsigned int skip)
{
	bool ok = true;
	if (skip == 0) {
		Assert(false);
		return false;
	}

	if (fCurrentBlockNumber+1 < GetNumberOfBlocks()) {
		if (fCurrentBlockNumber + skip >= fCasImage->GetNumberOfBlocks()) {
			ok = SeekEnd();
		} else {
			fCurrentBlockNumber += skip;
		}
	} else {
		ok = false;
	}
	SetupNewBlock();
	if (ok) {
		fTracer->IndicateCasBlockChanged();
	}
	return ok;
}

bool CasHandler::SeekPrevBlock(unsigned int skip)
{
	bool ok = true;
	if (skip == 0) {
		Assert(false);
		return false;
	}

	if (fCurrentBlockNumber > 0) {
		if (skip > fCurrentBlockNumber) {
			ok = SeekStart();
		} else {
			fCurrentBlockNumber -= skip;
		}
	} else {
		ok = false;
	}
	SetupNewBlock();
	if (ok) {
		fTracer->IndicateCasBlockChanged();
	}
	return ok;
}

bool CasHandler::SeekPrevPart()
{
	bool ok = true;
	unsigned int part = GetCurrentPartNumber();

	if (fPartsIdx[part] == fCurrentBlockNumber) {
		if (part > 0) {
			fCurrentBlockNumber = fPartsIdx[part-1];
		} else {
			ok = false;
		}
	} else {
		fCurrentBlockNumber = fPartsIdx[part];
	}
	SetupNewBlock();
	if (ok) {
		fTracer->IndicateCasBlockChanged();
	}
	return ok;
}

bool CasHandler::SeekNextPart()
{
	bool ok = true;
	unsigned int part = GetCurrentPartNumber();

	if (part+1 < GetNumberOfParts()) {
		fCurrentBlockNumber = fPartsIdx[part+1];
	} else {
		ok = false;
	}
	SetupNewBlock();
	if (ok) {
		fTracer->IndicateCasBlockChanged();
	}
	return ok;
}

bool CasHandler::CalculateWaitTime(struct timeval& tv)
{
	MiscUtils::TimestampType ts;
	ts = MiscUtils::GetCurrentTime();
	if (ts >= fBlockStartTime) {
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		return false;
	} else {
		MiscUtils::TimestampToTimeval(fBlockStartTime - ts, tv);
		return true;
	}
}

void CasHandler::AbortTapePlayback()
{
	if (fState == eInternalStatePlaying) {
		fSIOWrapper->EndTapeBlock();
	}
	fState = eInternalStateDone;
	fTracer->IndicateCasStateChanged();
}

CasHandler::EPlayResult CasHandler::DoPlaying()
{
	fd_set read_set;
	struct timeval tv;
	struct timeval* tvp;

	sigset_t orig_sigset, sigset;

        // setup signal mask to block all signals except KILL, STOP, INT, ALRM
	sigfillset(&sigset);
	sigdelset(&sigset,SIGKILL);
	sigdelset(&sigset,SIGSTOP);
	sigdelset(&sigset,SIGINT);
	sigdelset(&sigset,SIGALRM);

	while (true) {
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		tvp = &tv;

		switch (fState) {
		case eInternalStatePaused:
		case eInternalStateDone:
			//TRACE("state paused/done");
			return eWaitForStart;
			break;
		case eInternalStateWaiting:
			if (CalculateWaitTime(tv)) {
				//TRACE("waiting %ld.%6ld secs", tv.tv_sec, tv.tv_usec);
			} else {
				fState = eInternalStateStartPlaying;
				continue;
			}
			break;
		case eInternalStateStartPlaying:
			{
				int ret = fSIOWrapper->StartTapeBlock();
				if (ret) {
					AERROR("starting CAS block %d failed: %d", fCurrentBlockNumber + 1, ret);
					AbortTapePlayback();
					continue;
				} else {
					TRACE("start transmission of tape block %d", fCurrentBlockNumber + 1);
					fState = eInternalStatePlaying;
				}
			}
			break;
		case eInternalStatePlaying:
			{
				int len = GetCurrentBlockLength() - fCurrentBytePos;
				if (len <= 0) {
					AERROR("CAS block %d: length <= 0", fCurrentBlockNumber + 1);
					AbortTapePlayback();
					continue;
				}
				if (len > MAX_KERNEL_BLOCK_SIZE) {
					len = MAX_KERNEL_BLOCK_SIZE;
				}
				TRACE("transmit tape block part: offset = %d len = %d", fCurrentBytePos, len);

				sigprocmask(SIG_BLOCK, &sigset, &orig_sigset);
				int ret = fSIOWrapper->SendRawFrameNoWait((uint8_t*) (fCurrentCasBlock->GetData() + fCurrentBytePos), len);
				sigprocmask(SIG_SETMASK, &orig_sigset, NULL);

				fCurrentBytePos += len;
				if (ret) {
					AERROR("transmitting CAS block %d failed: %d ", fCurrentBlockNumber + 1, ret);
					AbortTapePlayback();
					continue;
				}
				if (fCurrentBytePos == GetCurrentBlockLength()) {
					if (fCurrentBlockNumber + 1 < GetNumberOfBlocks()) {
						fSIOWrapper->EndTapeBlock();
						fCurrentBlockNumber++;
						fState = eInternalStateWaiting;
						SetupNewBlock();
						fTracer->IndicateCasBlockChanged();
					} else {
						AbortTapePlayback();
					}
					continue;
				}
			}
			break;
		}

		FD_ZERO(&read_set);
		FD_SET(STDIN_FILENO, &read_set);

		int ret = select(STDIN_FILENO + 1, &read_set, NULL, NULL, tvp);
		//TRACE("select returned %d", ret);

		if (ret == -1) {
			return eGotSignal;
		}
		if (ret == 0) {
			if (fState == eInternalStateWaiting) {
				fState = eInternalStateStartPlaying;
			}
		}
		if (ret == 1) {
			if (!FD_ISSET(STDIN_FILENO, &read_set)) {
				Assert(false);
			}
			return eGotKeypress;
		}
	}
	Assert(false);
	return eGotKeypress;
}

void CasHandler::SetPause(bool on)
{
	if (on) {
		switch (fState) {
		case eInternalStatePlaying:
			fSIOWrapper->EndTapeBlock();
		case eInternalStateStartPlaying:
			fUnpauseState = fState;
			break;
		default:
			fUnpauseState = eInternalStateWaiting;
			break;
		}
		fState = eInternalStatePaused;
	} else {
		switch (fUnpauseState) {
		case eInternalStatePlaying:
			fSIOWrapper->StartTapeBlock();
			break;
		default:
			SetupNewBlock();
			break;
		}
		fState = fUnpauseState;
	}
	fTracer->IndicateCasStateChanged();
}

void CasHandler::SkipGap()
{
	if (fState == eInternalStateWaiting) {
		fState = eInternalStateStartPlaying;
	}
}

bool CasHandler::SetTapeSpeedPercent(unsigned int p)
{
	if (p == 0 || p >= 200) {
		AERROR("illegal tape speed percent %d", p);
		return false;
	} else {
		fTapeSpeedPercent = p;
		ALOG("set tape speed to %3d%%", p);
		return true;
	}
}
