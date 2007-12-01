#ifndef FILEINPUT_H
#define FILEINPUT_H

/*
   FileInput.h - extension of StringInput supporting filename completion

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

#include "StringInput.h"
#include "DirectoryCache.h"
#include <limits.h>

class FileInput: public StringInput {
public:
	FileInput(CursesFrontend* frontend, RCPtr<DirectoryCache>& dircache);
	virtual ~FileInput();

	virtual ECompletionResult TryComplete(bool lastActionWasAlsoComplete);
	virtual bool PrepareForSelection();
	virtual bool PrepareForFinish();

	void SplitInputIntoPathAndFilename();

	const char* GetFilename() { return fFilename; }
	const char* GetPath() { return fPath; }

	void SetEnableVirtualDriveKey(bool on);
	bool InputIsVirtualDrive() const;

protected:
	virtual bool IsEndOfInputChar(int ch);

private:
	typedef StringInput super;

	void ShowMatches();
	bool DirEntryMatchesFilename(DirEntry* e);

	RCPtr<DirectoryCache> fDirCache;

	char fFilename[NAME_MAX];
	char fPath[PATH_MAX];
	unsigned int fFilenameLen;

	ECompletionResult fLastCompletionResult;

	bool fEnableVirtualDriveKey;
	bool fInputIsVirtualDrive;
};

#endif
