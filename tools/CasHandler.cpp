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
#include <signal.h>

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

	fTracer = SIOTracer::GetInstance();

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
	fTracer->IndicateCasBlockChanged();
	return true;
}

bool CasHandler::SeekEnd()
{
	fCurrentBlockNumber = GetNumberOfBlocks() - 1;
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
			return SeekEnd();
		} else {
			fCurrentBlockNumber += skip;
		}
	} else {
		ok = false;
	}
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
			return SeekStart();
		} else {
			fCurrentBlockNumber -= skip;
		}
	} else {
		ok = false;
	}
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
	if (ok) {
		fTracer->IndicateCasBlockChanged();
	}
	return ok;
}

bool CasHandler::SeekNextPart()
{
	bool ok;
	unsigned int part = GetCurrentPartNumber();

	if (part+1 < GetNumberOfParts()) {
		fCurrentBlockNumber = fPartsIdx[part+1];
	} else {
		ok = false;
	}
	if (ok) {
		fTracer->IndicateCasBlockChanged();
	}
	return ok;
}

void CasHandler::SetupNextBlock()
{
	fBlockStartTime = MiscUtils::GetCurrentTime() + GetCurrentBlockGap() * 1000;
	fSIOWrapper->SetTapeBaudrate(GetCurrentBlockBaudRate());
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
		switch (fState) {
		case eInternalStatePaused:
		case eInternalStateDone:
			//TRACE("state paused/done");
			tvp = NULL; // wait forever;
			break;
		case eInternalStateWaiting:
			if (CalculateWaitTime(tv)) {
				//TRACE("waiting %ld.%6ld secs", tv.tv_sec, tv.tv_usec);
				tvp = &tv;
			} else {
				fState = eInternalStatePlaying;
			}
			break;
		case eInternalStatePlaying:
			break;
		}

		if (fState == eInternalStatePlaying) {
			//TRACE("sending block");
			// don't disturb us
			sigprocmask(SIG_BLOCK, &sigset, &orig_sigset);

			RCPtr<CasBlock> block = fCasImage->GetBlock(fCurrentBlockNumber);

			bool ok = true;
			if (block.IsNull()) {
				AERROR("getting CAS block %d failed", fCurrentBlockNumber + 1);
				ok = false;
			} else {
				unsigned int len = block->GetLength();
				if (len == 0 || len > 200) {
					AERROR("illegal CAS block length %d", len);
					ok = false;
				} else {
					int ret = fSIOWrapper->SendTapeBlock((unsigned char*) (block->GetData()), len);
					if (ret) {
						AERROR("transmitting CAS block %d failed", fCurrentBlockNumber + 1);
						ok = false;
					}
				}
			}
			if (ok) {
				if (fCurrentBlockNumber + 1 < GetNumberOfBlocks()) {
					fCurrentBlockNumber++;
				} else {
					ok = false;
				}
			}
			if (ok) {
				SetupNextBlock();
				fState = eInternalStateWaiting;
				fTracer->IndicateCasBlockChanged();
			} else {
				fState = eInternalStateDone;
				fTracer->IndicateCasStateChanged();
			}
			// setup original signal mask
			sigprocmask(SIG_SETMASK, &orig_sigset, NULL);
		} else {
			FD_ZERO(&read_set);
			FD_SET(STDIN_FILENO, &read_set);

			int ret = select(STDIN_FILENO + 1, &read_set, NULL, NULL, tvp);

			if (ret == -1) {
				return eGotSignal;
			}
			if (ret == 0) {
				if (fState == eInternalStateWaiting) {
					fState = eInternalStatePlaying;
				} else {
					AERROR("select timeout with illegal state %d", fState);
					fState = eInternalStateDone;
				}
			}
			if (ret == 1) {
				if (!FD_ISSET(STDIN_FILENO, &read_set)) {
					Assert(false);
				}
				return eGotKeypress;
			}
		}
	}
	Assert(false);
	return eGotKeypress;
}

void CasHandler::SetPause(bool on)
{
	if (on) {
		fState = eInternalStatePaused;
	} else {
		SetupNextBlock();
		fState = eInternalStateWaiting;
	}
	fTracer->IndicateCasStateChanged();
}
