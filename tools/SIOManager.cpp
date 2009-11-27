/*
   SIOManager.cpp - simple SIO command dispatcher

   Copyright (C) 2002-2004 Matthias Reichl <hias@horus.com>

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

#include <signal.h>
#include <stdio.h>

#include "SIOManager.h"

#include "AtariDebug.h"

#include "SIOTracer.h"
#include "DeviceManager.h"

SIOManager::SIOManager(const RCPtr<SIOWrapper>& wrapper)
	: fWrapper(wrapper)
{
}

SIOManager::~SIOManager()
{
}

bool SIOManager::RegisterHandler(uint8_t device_id, const RCPtr<AbstractSIOHandler>& handler)
{
	if (fHandlers[device_id]) {
		return false;
	} else {
		fHandlers[device_id] = handler;
		return true;
	}
}

bool SIOManager::UnregisterHandler(uint8_t device_id)
{
	if (fHandlers[device_id]) {
		fHandlers[device_id] = RCPtr<AbstractSIOHandler>();
		return true;
	} else {
		return false;
	}
}

int SIOManager::DoServing(int otherReadPollDevice)
{
	int ret;
	SIO_command_frame frame;
	sigset_t orig_sigset, sigset;

	// setup signal mask to block all signals except KILL, STOP, INT, ALRM
	sigfillset(&sigset);
	sigdelset(&sigset,SIGKILL);
	sigdelset(&sigset,SIGSTOP);
	sigdelset(&sigset,SIGINT);
	sigdelset(&sigset,SIGALRM);

	while (1) {
		ret = fWrapper->WaitForCommandFrame(otherReadPollDevice);
		switch (ret) {
		case 0: {
			sigprocmask(SIG_BLOCK, &sigset, &orig_sigset);
				// we don't want to be disturbed...
			
			ret=fWrapper->GetCommandFrame(frame);
		        if (ret == 0 ) {
				if (fHandlers[frame.device_id] && fHandlers[frame.device_id]->IsActive()) {
					ret = fHandlers[frame.device_id]->ProcessCommandFrame(frame, fWrapper);
				} else {
					SIOTracer::GetInstance()->TraceUnhandeledCommandFrame(frame);
				}
			} else {
				LOG_SIO_MISC("GetCommandFrame failed: %d", ret);
			}

			sigprocmask(SIG_SETMASK, &orig_sigset, NULL);
			break;
		}
		case -1: // timeout - process delayed tasks
			if (fHandlers[DeviceManager::eSIOPrinter]) {
				// flush printer buffer
				fHandlers[DeviceManager::eSIOPrinter]->ProcessDelayedTasks();
			}
			break;
		case 1:
			return 0;
		case 2:
			return 1;
		default:
			return -1;
		}
	}
}
