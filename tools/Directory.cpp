/*
   Directory.cpp - contains the filenames in a directory

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

#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>

#include "Directory.h"
#include "OS.h"
#include "AtariDebug.h"
#include "SIOTracer.h"

DirEntry::DirEntry()
	: fName(0), fType(eUnknown)
{
}

DirEntry::DirEntry(const char* name, EEntryType type, off_t bytesize)
	: fName(0), fLen(0), fType(type), fByteSize(bytesize)
{
	SetName(name);
}

DirEntry::~DirEntry()
{
	if (fName) {
		delete[] fName;
	}
}

void DirEntry::SetName(const char* string)
{
	if (fName) {
		delete[] fName;
	}
	if (string) {
		fLen = strlen(string);
		fName = new char[fLen+1];
		strcpy(fName, string);
	} else {
		fLen = 0;
		fName = 0;
	}
}

Directory::Directory()
	: fEntries(0), fSize(0), fAllocatedSize(0),
	  fFileselectorPosition(0)
{
}

Directory::~Directory()
{
	Init();
}

void Directory::Init()
{
	if (fEntries) {
		unsigned int i;
		for (i=0;i<fSize;i++) {
			delete fEntries[i];
		}
		delete[] fEntries;
	}
	fEntries = 0;
	fSize = 0;
	fAllocatedSize = 0;
}

void Directory::Realloc(unsigned int newsize)
{
	if (newsize >= fSize) {
		DirEntry** d;
		d = new DirEntry*[newsize];
		memcpy(d, fEntries, fSize * sizeof(DirEntry*));
		delete[] fEntries;
		fEntries = d;
		fAllocatedSize = newsize;
	}
}

DirEntry* Directory::Get(unsigned int num)
{
	if (num < fSize) {
		return fEntries[num];
	} else {
		return 0;
	}
}

void Directory::AddEntry(const char* name, DirEntry::EEntryType type, off_t bytesize)
{
	PrepareForIndex(fSize);
	fEntries[fSize] = new DirEntry(name, type, bytesize);
	fSize++;
}

static int compare_direntry(const void* a, const void* b)
{
	DirEntry* da = *((DirEntry**) a);
	DirEntry* db = *((DirEntry**) b);
	return strcasecmp(da->fName, db->fName);
}

int Directory::ReadDirectory(const char* path, bool sortDir, bool alwaysStatFiles)
{
	Init();

	DIR *dir = opendir(path);
	if (dir == NULL) {
		return -1;
	}

	unsigned int plen = strlen(path);
	struct stat statbuf;

	struct dirent* de;

	AddEntry("..", DirEntry::eParentDirectory, 0);
	
	while ( (de = readdir(dir)) != NULL) {
		if (   (strcmp(de->d_name,".") != 0)
		    && (strcmp(de->d_name,"..") != 0) ) {
			char * str = new char[plen + strlen(de->d_name) + 2];
			sprintf(str,"%s%c%s", path, DIR_SEPARATOR, de->d_name);
			if (sortDir || alwaysStatFiles) {
/*
#ifdef WINVER
				if (stat(str, &statbuf) == 0) {
#else
				if (lstat(str, &statbuf) == 0) {
#endif
*/
				if (stat(str, &statbuf) == 0) {
					if (S_ISREG(statbuf.st_mode)) {
						AddEntry(de->d_name, DirEntry::eFile, statbuf.st_size);
/*
#ifndef WINVER
					} else if (S_ISLNK(statbuf.st_mode)) {
						AddEntry(de->d_name, DirEntry::eLink, statbuf.st_size);
#endif
*/
					} else if (S_ISDIR(statbuf.st_mode)) {
						AddEntry(de->d_name, DirEntry::eDirectory, statbuf.st_size);
					}
				}
			} else {
				AddEntry(de->d_name, DirEntry::eUnknown, statbuf.st_size);
			}

			delete[] str;
		}
	}
	closedir(dir);
	Realloc(fSize);
	if ((sortDir) && (fSize > 2) ) {
		qsort(&(fEntries[1]), fSize-1, sizeof(DirEntry*), compare_direntry);
	}
	return fSize;
}

int Directory::Find(const char* name, unsigned int startPos)
{
	if (name == NULL) {
		return -1;
	}
	unsigned int len = strlen(name);
	if (len == 0) {
		if (startPos < fSize) {
			return startPos;
		} else {
			return -1;
		}
	}
	unsigned int i = startPos;
	while (i < fSize) {
		if (strncasecmp(fEntries[i]->fName, name, len) == 0) {
			return i;
		}
		i++;
	}
	return -1;
}
