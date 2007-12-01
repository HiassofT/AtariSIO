#ifndef DIRECTORYCACHE_H
#define DIRECTORYCACHE_H

/*
   DirectoryCache.h - simple directory-cache

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

#include "Directory.h"
#include "RefCounted.h"
#include "RCPtr.h"

#include <sys/types.h>

class DirectoryCache : public RefCounted {
public:
	DirectoryCache();
	virtual ~DirectoryCache();

	enum EDirectoryMode {
		eDirectoryUnsorted,
		eDirectorySorted,
		eDirectoryUnsortedOrSorted
	};
	RCPtr<Directory> GetDirectory(const char* path, EDirectoryMode dirMode);

	// only clear cached directory data but keep meta information
	void ClearDirectoryData();
	// completely flush the cache
	void ClearCache();

private:
	RCPtr<Directory> fCachedDirectory;
	char* fCachedPath;
	time_t fCachedModificationTime;
	EDirectoryMode fCachedDirectoryMode;
	unsigned int fCachedFileselectorPosition;
};

#endif
