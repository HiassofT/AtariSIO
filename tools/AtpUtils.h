#ifndef ATPUTILS_H
#define ATPUTILS_H

/*
   AtpUtils.h - misc helper routines for the ATP format

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

#include <sys/types.h>

#include "AtpImage.h"
#include "AtrImage.h"

namespace AtpUtils {

	RCPtr<AtpImage> CreateAtpImageFromAtrImage(RCPtr<AtrImage> atrImage);

};

#endif

