#ifndef FILESELECT_H
#define FILESELECT_H

/*
   FileSelect.h - full screen file selection

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

#include <curses.h>
#include <limits.h>

#include "CursesFrontend.h"
#include "Directory.h"

class FileSelect {
public:
	FileSelect(CursesFrontend* frontend);
	~FileSelect();

	bool Select(WINDOW* win,
		const char* path, const char* seekfirst,
		char * selection, unsigned int maxlen);

	void SetEnableVirtualDriveKey(bool on);
	void SetEnableDos2xDirectory(bool on);
	bool InputIsVirtualDrive() const;

private:
	bool InitScreenSize();

	bool Init();
	void InitScreen();

	void GotoPosition(unsigned int pos);
	void PlaceCursorTop();

	void CheckUpperMargin(bool positionCursorTop);
	void CheckLowerMargin(bool positionCursorTop);

	void DisplayDir(bool fullRedraw = false);
	void DisplayDirEntry(unsigned int entry, bool highlight);

	void DisplaySearchFilename();
	void ClearSearchFilename();

	DirEntry::EEntryType BuildFilename(char* filename);

	CursesFrontend* fFrontend;
	WINDOW* fWindow;
	WINDOW* fInputLineWindow;
	unsigned int fColumns, fLines;
	unsigned int fPosition, fDisplayStart;
	unsigned int fScrollMargin;
	unsigned int fOldPosition, fOldDisplayStart;

	attr_t fColorStandard;
	attr_t fColorSelected;

	char fPath[PATH_MAX];
	RCPtr<Directory> fDirectory;
	unsigned int fDirectorySize;

	char fSearchFilename[NAME_MAX];
	unsigned int fSearchFilenameLength;
	unsigned int fSearchFilenameMaxLength;

	bool fEnableVirtualDriveKey;
	bool fInputIsVirtualDrive;
	bool fEnableDos2xDirectory;
};

#endif
