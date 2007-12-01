/*
   DirectoryCache.cpp - simple directory-cache

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

#include "DirectoryCache.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include "SIOTracer.h"
#include "AtariDebug.h"

DirectoryCache::DirectoryCache()
	: fCachedPath(0)
{
}

DirectoryCache::~DirectoryCache()
{
	ClearCache();
}

void DirectoryCache::ClearDirectoryData()
{
	if (fCachedDirectory) {
		// remember last fileselector position
		fCachedFileselectorPosition = fCachedDirectory->GetFileselectorPosition();
	}
	fCachedDirectory = 0;
}

void DirectoryCache::ClearCache()
{
	ClearDirectoryData();
	if (fCachedPath) {
		delete[] fCachedPath;
	}
	fCachedPath = 0;
	fCachedFileselectorPosition = 0;
}

RCPtr<Directory> DirectoryCache::GetDirectory(const char* path, EDirectoryMode dirMode)
{
	char p[PATH_MAX];
	if (realpath(path, p) == NULL) {
		return RCPtr<Directory>();
	}
	struct stat statbuf;
	if (stat(p, &statbuf) != 0) {
		return RCPtr<Directory>();
	}
	//DPRINTF("cachedPath: %s path: %s", fCachedPath, p);
	//DPRINTF("cachedDirMode: %d dirMode: %d", fDirectoryMode, dirMode);
	
	unsigned int fileselectPos = 0;
	bool setFileselectPos = false;

	if (fCachedPath && (strcmp(p, fCachedPath) == 0)) {
		if ( (dirMode == fCachedDirectoryMode) || (dirMode == eDirectoryUnsortedOrSorted) ) {
			if (statbuf.st_mtime == fCachedModificationTime ) {
				if (fCachedDirectory) {
					//DPRINTF("DirCache hit: %s", fCachedPath);
					return fCachedDirectory;
				} else {
					//DPRINTF("DirCache metadata hit: %s, pos=%d", fCachedPath, fCachedFileselectorPosition);
					// metatdata matches, rebuild directory and restore
					// fileselector position
					fileselectPos = fCachedFileselectorPosition;
					setFileselectPos = true;
				}
			} else {
				//DPRINTF("DirCache invalidated - directory changed");
			}
		}
	}
	if (fCachedPath) {
		delete[] fCachedPath;
	}
	fCachedPath = new char[strlen(p)+1];
	strcpy(fCachedPath, p);
	//DPRINTF("DirCache miss: %s", fCachedPath);
	if (dirMode == eDirectoryUnsortedOrSorted) {
		dirMode = eDirectoryUnsorted;
	}
	fCachedDirectoryMode = dirMode;
	fCachedModificationTime = statbuf.st_mtime;
	
	fCachedDirectory = new Directory;

	if (fCachedDirectory->ReadDirectory(fCachedPath, (fCachedDirectoryMode == eDirectorySorted) ) < 0) {
		fCachedDirectory = 0;
	}
	if (fCachedDirectory && setFileselectPos) {
		//DPRINTF("setting fileselector position to %d", fileselectPos);
		fCachedDirectory->SetFileselectorPosition(fileselectPos);
	}
	return fCachedDirectory;
}
