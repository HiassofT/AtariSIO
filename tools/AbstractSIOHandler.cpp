/*
   AbstractSIOHandler.cpp - a (almost pure) virtual base class for
   SIO handlers

   Copyright (C) 2003, 2004 Matthias Reichl <hias@horus.com>

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

AbstractSIOHandler::AbstractSIOHandler()
: fIsActive(true)
{ }

bool AbstractSIOHandler::IsAtrSIOHandler() const
{
	return false;
}

bool AbstractSIOHandler::IsAtpSIOHandler() const
{
	return false;
}

bool AbstractSIOHandler::IsPrinterHandler() const
{
	return false;
}

bool AbstractSIOHandler::IsRemoteControlHandler() const
{
	return false;
}

void  AbstractSIOHandler::ProcessDelayedTasks(bool /*isForced*/)
{
}
