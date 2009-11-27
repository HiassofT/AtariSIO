#ifndef DIRECTORY_H
#define DIRECTORY_H

/*
   Directory.h - contains the filenames in a directory

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

#include "RefCounted.h"
#include "RCPtr.h"
#include <sys/types.h>

class DirEntry {
public:
	enum EEntryType {
		eUnknown,
		eFile,
		eDirectory,
		eParentDirectory,
		eLink
	};

	DirEntry();
	DirEntry(const char* name, EEntryType type, off_t bytesize);

	~DirEntry();

	void SetName(const char* string);

	bool IsDirectory()
	{
		return (fType == eDirectory) || (fType==eParentDirectory);
	}	

	bool IsLink()
	{
		return (fType == eLink);
	}	

	char* fName;
	unsigned int fLen;
	EEntryType fType;
	off_t fByteSize;
};

class Directory : public RefCounted {
public:
	Directory();
	virtual ~Directory();

	int ReadDirectory(const char* path, bool sortDir, bool alwaysStatFiles = false);

	unsigned int Size() { return fSize; }

	DirEntry* Get(unsigned int num);

	int Find(const char* name, unsigned int startPos = 0);

	//hack for storing the current position in the file selector
	inline unsigned int GetFileselectorPosition() const;
	inline void SetFileselectorPosition(unsigned int pos);

private:
	void Init();
	void Realloc(unsigned int newsize);
	inline void PrepareForIndex(unsigned int num);
	void AddEntry(const char* name, DirEntry::EEntryType type, off_t bytesize);

	DirEntry** fEntries;
	unsigned int fSize;
	unsigned int fAllocatedSize;

	unsigned int fFileselectorPosition;
};

inline void Directory::PrepareForIndex(unsigned int num)
{
	if (num >= fAllocatedSize) {
		if (fAllocatedSize) {
			Realloc(num * 2);
		} else {
			Realloc(10);
		}
	}
}

inline unsigned int Directory::GetFileselectorPosition() const
{
	return fFileselectorPosition;
}

inline void Directory::SetFileselectorPosition(unsigned int pos)
{
	fFileselectorPosition = pos;
}


#endif

