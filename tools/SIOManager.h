#ifndef SIOMANAGER_H
#define SIOMANAGER_H

/*
   SIOManager.h - simple SIO command dispatcher

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


#include "AbstractSIOHandler.h"
#include "SIOWrapper.h"

class SIOManager : public RefCounted {
public:
	SIOManager(const RCPtr<SIOWrapper>& wrapper);
	virtual ~SIOManager();

	bool RegisterHandler(uint8_t device_id, const RCPtr<AbstractSIOHandler>& handler);
	RCPtr<AbstractSIOHandler>& GetHandler(uint8_t device_id);
	RCPtr<const AbstractSIOHandler> GetConstHandler(uint8_t device_id) const;

	bool UnregisterHandler(uint8_t device_id);

	/*
	 * return:
	 * -1 = an error occurred
	 *  0 = specified device has data available
	 *  1 = error (or signal caught) during select
	 */
	int DoServing(int otherReadPollDevice=-1);

private:
	RCPtr<SIOWrapper> fWrapper;
	RCPtr<AbstractSIOHandler> fHandlers[256];
	
	// debugging stuff (default = off)
};

inline RCPtr<AbstractSIOHandler>& SIOManager::GetHandler(uint8_t device_id)
{
	return fHandlers[device_id];
}

inline RCPtr<const AbstractSIOHandler> SIOManager::GetConstHandler(uint8_t device_id) const
{
	return fHandlers[device_id];
}

#endif
