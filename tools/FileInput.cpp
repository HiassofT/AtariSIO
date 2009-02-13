/*
   FileInput.cpp - extension of StringInput supporting filename completion

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
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>

#include "OS.h"
#include "FileInput.h"
#include "SIOTracer.h"
#include "DirectoryCache.h"
#include "AtariDebug.h"
#include "CursesFrontend.h"

FileInput::FileInput(CursesFrontend* frontend, RCPtr<DirectoryCache>& dirCache)
	: super(frontend),
	  fDirCache(dirCache),
	  fLastCompletionResult(eNoComplete),
	  fEnableVirtualDriveKey(false),
	  fInputIsVirtualDrive(false)
{
}

FileInput::~FileInput()
{
}

void FileInput::SplitInputIntoPathAndFilename()
{
	int sepPos = fEditPos - 1;
	while ( (sepPos >= 0) && (fString[sepPos] != DIR_SEPARATOR) ) {
		sepPos--;
	}

	if (sepPos == -1) {
		strcpy(fPath,".");
		strncpy(fFilename, fString, NAME_MAX-1);
		fFilename[NAME_MAX-1] = 0;
		if (fEditPos < NAME_MAX-1 ) {
			fFilename[fEditPos] = 0;
		}
	} else {
		if (sepPos == 0) {
			strncpy(fPath,"/",PATH_MAX-1);
		} else {
			strncpy(fPath, fString, PATH_MAX-1);
			fPath[PATH_MAX-1] = 0;
			if (sepPos < PATH_MAX-1) {
				fPath[sepPos] = 0;
			}
		}
		strncpy(fFilename, fString + sepPos + 1, NAME_MAX-1);
		fFilename[NAME_MAX-1] = 0;
		if (fEditPos - sepPos - 1 < NAME_MAX-1) {
			fFilename[fEditPos - sepPos - 1] = 0;
		}
	}
	if ( (fPath[0]=='~') && ((fPath[1]==DIR_SEPARATOR) || (fPath[1]==0)) ) {
		char *home = getenv("HOME");
		if (home == NULL) {
			struct passwd *pw = getpwuid(getuid());
			home = pw->pw_dir;
		}
		if (home != NULL) {
			unsigned int hlen = strlen(home);
			unsigned int plen = strlen(fPath);
			if (plen - 1 + hlen < PATH_MAX) {
				memmove(fPath + hlen - 1, fPath, plen);
				memcpy(fPath, home, hlen);
			}
		}
	}
	fFilenameLen = strlen(fFilename);
}

bool FileInput::PrepareForFinish()
{
	if ( (fString[0]=='~') && (fString[1]==DIR_SEPARATOR) ) {
		char *home = getenv("HOME");
		if (home == NULL) {
			struct passwd *pw = getpwuid(getuid());
			home = pw->pw_dir;
		}
		if (home != NULL) {
			char tmp[fCurrentStringLength + strlen(home)];
			strcpy(tmp, home);
			strcat(tmp, fString + 1);
			strncpy(fString, tmp, fMaxStringLength);
			fString[fMaxStringLength - 1] = 0;
		} else {
			return false;
		}
	}
	return true;
}

void FileInput::ShowMatches()
{
	RCPtr<Directory> dir = fDirCache->GetDirectory(fPath, DirectoryCache::eDirectorySorted);

	if (dir.IsNull()) {
		fLastCompletionResult = eNoComplete;
		return;
	}

	// count matches
	unsigned int matches = 0;
	unsigned int maxMatchLen = 0;

	unsigned int maxEntry = dir->Size();

	for (unsigned int pos = 0; pos < maxEntry; pos++) {
		DirEntry* e = dir->Get(pos);
		if (DirEntryMatchesFilename(e)) {
			matches++;
			unsigned int len = strlen(e->fName);
			if (e->IsDirectory() || e->IsLink()) {
				len++;
			} 
		       	if (len > maxMatchLen) {
				maxMatchLen = len;
			}
		}
	}
	if (matches == 0) {
		AWARN("No matches found for \"%s*\"!", fFilename);
	} else {
		ALOG("Possible matches are:");

		unsigned int screenWidth = fFrontend->GetScreenWidth();
		unsigned int columnWidth = maxMatchLen+1;
		unsigned int columns = screenWidth / (columnWidth);

		if (columns == 0) {
			columns = 1;
		}

		char* tmpstr = new char[columns * columnWidth+1];

		unsigned int col = 0;
		unsigned int lastlen = 0;

		for (unsigned int pos = 0; pos < maxEntry; pos++) {
			DirEntry* e = dir->Get(pos);
			if (DirEntryMatchesFilename(e)) {
				if (col > 0) {
					memset(tmpstr+(col-1)*columnWidth+lastlen, ' ', columnWidth-lastlen);
				}
				lastlen = strlen(e->fName);
				unsigned int pos = col*columnWidth;
				strcpy(tmpstr+pos,e->fName);
				if (e->IsDirectory()) {
					tmpstr[pos+lastlen++] = DIR_SEPARATOR;
					tmpstr[pos+lastlen] = 0;
				}
				if (e->IsLink()) {
					tmpstr[pos+lastlen++] = '@';
					tmpstr[pos+lastlen] = 0;
				}
				col++;
				if (col == columns) {
					col = 0;
					ALOG("%s", tmpstr);
				}
			}
		}
		if (col != 0) {
			ALOG("%s", tmpstr);
		}
		delete[] tmpstr;
	}
}

bool FileInput::DirEntryMatchesFilename(DirEntry* e)
{
	if (fFilenameLen == 0) {
		return (e->fName[0] != '.');
	} else {
		return (strncmp(fFilename, e->fName, fFilenameLen) == 0);
	}
}

StringInput::ECompletionResult FileInput::TryComplete(bool lastActionWasAlsoComplete)
{
	if (fDirCache.IsNull()) {
		return eNoComplete;
	}

	if ( (fEditPos == 1) && (fString[0] == '~') ) {
		if (InsertChar(DIR_SEPARATOR)) {
			fLastCompletionResult = eFullComplete;
			return fLastCompletionResult;
		} else {
			fLastCompletionResult = eNoComplete;
			return fLastCompletionResult;
		}
	}
	SplitInputIntoPathAndFilename();

	if (lastActionWasAlsoComplete && (fLastCompletionResult != eFullComplete) ) {
		ShowMatches();
		return fLastCompletionResult;
	}

	RCPtr<Directory> dir = fDirCache->GetDirectory(fPath, DirectoryCache::eDirectorySorted);

	if (dir.IsNull()) {
		fLastCompletionResult = eNoComplete;
		return fLastCompletionResult;
	}

	unsigned int dirSize = dir->Size();
	char matchString[NAME_MAX];
	matchString[0] = 0;

	bool first = true;
	fLastCompletionResult = StringInput::eNoComplete;

	for (unsigned int num=0;num<dirSize;num++) {
		DirEntry* e = dir->Get(num);
		bool match = DirEntryMatchesFilename(e);
		if (match) {
			if (first) {
				strncpy(matchString, e->fName, NAME_MAX-1);
				matchString[NAME_MAX-1] = 0;
				first = false;

				char fullPath[PATH_MAX];
				snprintf(fullPath, PATH_MAX-1, "%s%c%s", fPath, DIR_SEPARATOR, e->fName);
				fullPath[PATH_MAX-1] = 0;

				struct stat statbuf;
				if (stat(fullPath, &statbuf) == 0) {
					if (S_ISDIR(statbuf.st_mode)) {
						strcat(matchString,"/");
					}
				}
				fLastCompletionResult = StringInput::eFullComplete;
			} else {
				int i = fFilenameLen;
				while (matchString[i] && (matchString[i] == e->fName[i])) {
					i++;
				}
				matchString[i] = 0;
				fLastCompletionResult = StringInput::ePartialComplete;
			}
		}
	}

	if (strlen(matchString) > fFilenameLen) {
		if (!InsertString(matchString+fFilenameLen)) {
			fLastCompletionResult = StringInput::eNoComplete;
		}
	}
	return fLastCompletionResult;
}

bool FileInput::PrepareForSelection()
{
	SplitInputIntoPathAndFilename();

	struct stat sb;
	if ( (stat(fPath,&sb) == 0) && (S_ISDIR(sb.st_mode)) ) {
		return true;
	}
	strcpy(fPath,".");

	return true;
}

bool FileInput::InputIsVirtualDrive() const
{
	return fInputIsVirtualDrive;
}

bool FileInput::IsEndOfInputChar(int ch)
{
	if (super::IsEndOfInputChar(ch)) {
		fInputIsVirtualDrive = false;
		return true;
	} else {
		if (fEnableVirtualDriveKey && (ch == CursesFrontend::eVirtualDriveKey) ) {
			fInputIsVirtualDrive = true;
			// control-V - virtual PC mirror
			return true;
		}
	}
	return false;

}

void FileInput::SetEnableVirtualDriveKey(bool on)
{
	fEnableVirtualDriveKey = on;
}
