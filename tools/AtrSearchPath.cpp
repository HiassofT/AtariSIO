/*
   AtrSearchPath - search for file in ATRPATH environment variable

   relocator copyright (c) 2002 Matthias Reichl <hias@horus.com>
   6502 code (c) ABBUC e.V. (www.abbuc.de)

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

#include "AtrSearchPath.h"
#include "AtariDebug.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

AtrSearchPath* AtrSearchPath::fInstance = 0;

AtrSearchPath::AtrSearchPath()
	: super(getenv("ATRPATH"))
{
}

AtrSearchPath::~AtrSearchPath()
{
}

