/*
   FileSelect.cpp - full screen file selection

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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "OS.h"
#include "FileSelect.h"
#include "AtariDebug.h"
#include "MiscUtils.h"
#include "SIOTracer.h"
#include "CursesFrontend.h"

using namespace MiscUtils;

FileSelect::FileSelect(CursesFrontend *frontend)
	: fFrontend(frontend),
	  fEnableVirtualDriveKey(false),
	  fInputIsVirtualDrive(false),
	  fEnableDos2xDirectory(false)
{
	fInputLineWindow = fFrontend->GetInputLineWindow();
}

FileSelect::~FileSelect()
{
}

bool FileSelect::InitScreenSize()
{
	int x,y;
	getmaxyx(fWindow, y, x);
	fLines = y;
	fColumns = x;
	if ( (fLines < 1) || (fColumns < 10) ) {
		DPRINTF("FileSelect::InitScreenSize: screen is too small!");
		return false;
	}

	if (fLines >= 10) {
		fScrollMargin = 1;
	} else {
		fScrollMargin = 0;
	}

	if (fColumns - 9 >= NAME_MAX-1) {
		fSearchFilenameMaxLength = NAME_MAX-1;
	} else {
		fSearchFilenameMaxLength = fColumns - 9;
	}

	if (fSearchFilenameLength > fSearchFilenameMaxLength) {
		fSearchFilenameLength = fSearchFilenameMaxLength;
		fSearchFilename[fSearchFilenameLength] = 0;
	}
	return true;
}

bool FileSelect::Select(WINDOW* win, 
	const char* path, const char* seekfirst,
	char * selection, unsigned int maxlen)
{
	fInputIsVirtualDrive = false;
	fWindow = win;

	if (fFrontend->GotSigWinCh()) {
		fFrontend->HandleResize(true);
	}

	fSearchFilenameLength = 0;

	if (!InitScreenSize()) {
		return false;
	}
	fColorStandard = fFrontend->GetAuxColorStandard();
	fColorSelected = fFrontend->GetAuxColorSelected();

	if ( (path != NULL) && (path[0] != 0) ) {
		strncpy(fPath, path, PATH_MAX-1);
		fPath[PATH_MAX-1] = 0;
	} else {
		fPath[0] = '.';
		fPath[1] = 0;
	}

	if (!Init()) {
		return false;
	}
	if (seekfirst && *seekfirst) {
		strncpy(fSearchFilename, seekfirst, fSearchFilenameMaxLength);
		fSearchFilename[fSearchFilenameMaxLength-1] = 0;
		int pos = fDirectory->Find(fSearchFilename);
		if (pos >= 0) {
			fSearchFilenameLength = strlen(fSearchFilename);
			GotoPosition(pos);
			DisplayDir();
		} else {
			fSearchFilename[0] = 0;
			fSearchFilenameLength = 0;
		}
	} else {
		fSearchFilename[0] = 0;
		fSearchFilenameLength = 0;
	}
	DisplaySearchFilename();
	fFrontend->UpdateScreen();

	while(1) {
		int ch = fFrontend->GetCh(false);
		switch (ch) {
		case KEY_RESIZE:
			fFrontend->HandleResize(true);
			if (!InitScreenSize()) {
				return false;
			}
			fDisplayStart = 0;
			CheckLowerMargin(true);
			InitScreen();
			fFrontend->UpdateScreen();
			break;
		case KEY_UP:
			if (fPosition > 0) {
				fPosition --;
				CheckUpperMargin(true);
			} else {
				beep();
			}
			ClearSearchFilename();
			break;
		case KEY_DOWN:
			if (fPosition < fDirectorySize - 1) {
				fPosition++;
				CheckLowerMargin(false);
			} else {
				beep();
			}
			ClearSearchFilename();
			break;
		case KEY_PPAGE:
			if (fPosition > 0) {
				fPosition = fDisplayStart;
				CheckUpperMargin(false);
			} else {
				beep();
			}
			ClearSearchFilename();
			break;
		case KEY_NPAGE:
			if (fPosition < fDirectorySize - 1) {
				if (fDisplayStart + fLines - 1 >= fDirectorySize - 1) {
					fPosition = fDirectorySize - 1;
				} else {
					fPosition = fDisplayStart + fLines - 1;
				}
				CheckLowerMargin(true);
			} else {
				beep();
			}
			ClearSearchFilename();
			break;
		case KEY_HOME:
		case KEY_FIND:
		case 1: // ^A
			fPosition = 0;
			CheckUpperMargin(true);
			ClearSearchFilename();
			break;
		case KEY_END:
		case KEY_SELECT:
		case 5: // ^E
			fPosition = fDirectorySize - 1;
			CheckLowerMargin(false);
			ClearSearchFilename();
			break;
		case 27: // ESC
		case 7:  // ^G
			return false;
		case KEY_ENTER:
		case 13:
			{
				ClearSearchFilename();
				char fn[PATH_MAX];

				switch (BuildFilename(fn)) {
				case DirEntry::eDirectory:
					strncpy(fPath, fn, PATH_MAX-1);
					fPath[PATH_MAX-1] = 0;
					if (!Init()) {
						return false;
					}
					fFrontend->UpdateScreen();
					break;
				case DirEntry::eFile:
					strncpy(selection, fn, maxlen);
					if (chdir(fPath) != 0) {
						DPRINTF("chdir(\"%s\") failed", fPath);
					} else {
						SIOTracer::GetInstance()->IndicateCwdChanged();
					}
					fDirectory->SetFileselectorPosition(fPosition);
					return true;
				default:
					beep();
					break;
				}
			}
			break;
		case CursesFrontend::eVirtualDriveKey:
			if (fEnableVirtualDriveKey) {
				fInputIsVirtualDrive = true;
				strncpy(selection, fPath, maxlen);
				return true;
			}
			break;
		case KEY_BACKSPACE:
		case 8:
			if (fSearchFilenameLength > 0) {
				fSearchFilename[--fSearchFilenameLength] = 0;
				if (fSearchFilenameLength > 0) {
					int pos = fDirectory->Find(fSearchFilename);
					if (pos == -1) {
						DPRINTF("this should not happen - error in reverse search");
					} else {
						GotoPosition((unsigned int)pos);
					}
				}
				DisplaySearchFilename();
			} else {
				beep();
			}
			break;
		case 9: // TAB: goto next search item
			{
				int pos = -1;
				if (fPosition + 1 < fDirectorySize) {
					pos = fDirectory->Find(fSearchFilename, fPosition+1);
				}
				if (pos == -1) {
					pos = fDirectory->Find(fSearchFilename);
				}
				if (pos != -1) {
					if ((unsigned int)pos != fPosition) {
						GotoPosition((unsigned int)pos);
					} else {
						beep();
					}
				} else if (fSearchFilenameLength) {
					DPRINTF("Find returned -1 but SearchFilenameLength > 0");
				}
			}
			break;
		case 4: // ^D - show directory
			if (fEnableDos2xDirectory) {
				char fn[PATH_MAX];
				if (BuildFilename(fn) == DirEntry::eFile) {
					RCPtr<DiskImage> image = DeviceManager::LoadDiskImage(fn, true);
					if (image.IsNotNull()) {
						werase(fInputLineWindow);
						if (!fFrontend->ShowDos2Directory(image, true)) {
							beep();
						}
						unsigned int x,y;
						getmaxyx(fWindow, y, x);
						if ( (x != fColumns) || (y != fLines) ) {
							if (!InitScreenSize()) {
								return false;
							}
							fDisplayStart = 0;
							CheckLowerMargin(true);
						}
						InitScreen();
					} else {
						beep();
					}
				} else {
					beep();
				}
			} else {
				beep();
			}
			break;
		case 21: // ^U - clear input
			fSearchFilenameLength = 0;
			fSearchFilename[0] = 0;
			DisplaySearchFilename();
			break;
		default:
			if (ch >= 32 && ch <= 255) {
				if (fSearchFilenameLength < fSearchFilenameMaxLength) {
					fSearchFilename[fSearchFilenameLength++] = ch;
					fSearchFilename[fSearchFilenameLength] = 0;

					int pos = fDirectory->Find(fSearchFilename, fPosition);
					if ( (pos == -1) && (fPosition != 0) ) {
						pos = fDirectory->Find(fSearchFilename);
					}
					if (pos == -1) {
						fSearchFilename[--fSearchFilenameLength] = 0;
						beep();
					} else {
						GotoPosition((unsigned int)pos);
						DisplaySearchFilename();
					}
				} else {
					beep();
				}
			} else {
				beep();
			}
			break;
		}
		DisplayDir();
		fFrontend->UpdateScreen();
	}
}

bool FileSelect::Init()
{
	char rp[PATH_MAX];
	if (realpath(fPath, rp) == NULL) {
		DPRINTF("realpath(\"%s\") failed!", fPath);
		return false;
	}
	strcpy(fPath, rp);

	fDirectory = fFrontend->GetDirectoryCache()->GetDirectory(fPath, DirectoryCache::eDirectorySorted);
	if (fDirectory.IsNull()) {
		DPRINTF("reading directory \"%s\" failed!", fPath);
		return false;
	}

	fPosition = fDirectory->GetFileselectorPosition();
	fOldPosition = 0;
	fDisplayStart = fOldDisplayStart = 0;
	fDirectorySize = fDirectory->Size();

	fSearchFilename[0] = 0;
	fSearchFilenameLength = 0;

	InitScreen();

	return true;
}

void FileSelect::InitScreen()
{
	fFrontend->SetTopLineFilename(fPath, true);
	
	wbkgdset(fWindow, fColorStandard | ' ');
	werase(fWindow);

	DisplayDir(true);

	DisplaySearchFilename();

	if (fEnableVirtualDriveKey) {
		if (fEnableDos2xDirectory) {
			fFrontend->ShowHint("<return>=select, '^D'=DOS 2.x directory, '^V'=virtual drive, '^G'=abort");
		} else {
			fFrontend->ShowHint("<return>=select, '^V'=virtual drive, '^G'=abort");
		}
	} else {
		if (fEnableDos2xDirectory) {
			fFrontend->ShowHint("<return>=select, '^D'=DOS 2.x directory, '^G'=abort");
		} else {
			fFrontend->ShowHint("<return>=select, '^G'=abort");
		}
	}
}

void FileSelect::CheckUpperMargin(bool positionCursorTop)
{
	if (fDirectorySize <= fLines) {
		fDisplayStart = 0;
	} else {
		if (fPosition < fDisplayStart + fScrollMargin) {
			if (positionCursorTop) {
				if (fPosition <= fScrollMargin) {
					fDisplayStart = 0;
				} else {
					fDisplayStart = fPosition - fScrollMargin;
				}
			} else {
				if (fPosition <= fLines - fScrollMargin - 1) {
					fDisplayStart = 0;
				} else {
					fDisplayStart = fPosition - (fLines - fScrollMargin - 1);
				}
			}
		}
	}
}

void FileSelect::CheckLowerMargin(bool positionCursorTop)
{
	if (fPosition >= fDirectorySize) {
		DPRINTF("cursor position larger than directory size!");
		fPosition = fDirectorySize - 1;
	}
	if (fDirectorySize <= fLines) {
		fDisplayStart = 0;
	} else {
		if (fPosition > fDisplayStart + fLines - fScrollMargin - 1) {
			if (positionCursorTop) {
				if (fPosition < fDirectorySize - (fLines - fScrollMargin - 1) ) {
					fDisplayStart = fPosition - fScrollMargin;
				} else {
					fDisplayStart = fDirectorySize - fLines;
				}
			} else {
				if (fPosition < fDirectorySize - fScrollMargin) {
					fDisplayStart = fPosition - (fLines - fScrollMargin - 1);
				} else {
					fDisplayStart = fDirectorySize - fLines;
				}
			}
		}
	}
}

void FileSelect::GotoPosition(unsigned int pos)
{
	if (pos >= fDirectorySize) {
		DPRINTF("illegal position in GotoPosition");
		return;
	}
	fPosition = pos;
	CheckUpperMargin(true);
	CheckLowerMargin(true);
}

void FileSelect::DisplayDir(bool fullRedraw)
{
	if (fPosition >= fDirectorySize) {
		DPRINTF("position larger than directory size!");
		fPosition = fDirectorySize - 1;
	}

	if (fDirectorySize <= fLines) {
		fDisplayStart = 0;
	} else {
		if ( (fPosition < fDisplayStart) || (fPosition > fDisplayStart + fLines - 1) ) {
			DPRINTF("cursor position outside display area (%d : %d..%d", fPosition, fDisplayStart, fDisplayStart + fLines - 1);
			CheckUpperMargin(true);
			CheckLowerMargin(false);
		}
	}

	if (fDisplayStart != fOldDisplayStart) {
		fullRedraw = true;
	}

	if (fullRedraw) {
		unsigned int i;
		for (i = 0; i < fLines; i++) {
			DisplayDirEntry(i, fPosition == (fDisplayStart + i));
		}
	} else {
		DisplayDirEntry(fOldPosition - fDisplayStart, false);
		DisplayDirEntry(fPosition - fDisplayStart, true);
	}
	fOldDisplayStart = fDisplayStart;
	fOldPosition = fPosition;
}

void FileSelect::DisplayDirEntry(unsigned int relativePos, bool highlight)
{
	if ( relativePos >= fLines ) {
		DPRINTF("invalid position in DisplayDirEntry");
		return;
	}
	wmove(fWindow, relativePos, 0);
	if (relativePos + fDisplayStart >= fDirectorySize) {
		wclrtoeol(fWindow);
	} else {
		DirEntry* e = fDirectory->Get(fDisplayStart + relativePos);
		if (e == 0) {
			DPRINTF("error getting directory entry");
		} else {
			if (highlight) {
				wbkgdset(fWindow, fColorSelected | ' ');
				wattrset(fWindow, fColorSelected);
			} else {
				wbkgdset(fWindow, fColorStandard | ' ');
				wattrset(fWindow, fColorStandard);
			}
			unsigned int len = e->fLen;
			waddnstr(fWindow, e->fName, fColumns);
			if (len < fColumns) {
				if (e->IsDirectory()) {
					waddstr(fWindow, "/");
					len++;
				} else if (e->IsLink()) {
					waddstr(fWindow, "@");
					len++;
				}

				if (len < fColumns) {
					wclrtoeol(fWindow);
				}
			}
			if (highlight) {
				wbkgdset(fWindow, fColorStandard | ' ');
				wattrset(fWindow, fColorStandard);
			}
		}
	}
}

void FileSelect::DisplaySearchFilename()
{
	wmove(fInputLineWindow, 0, 0);
	waddstr(fInputLineWindow, "search: ");
	waddstr(fInputLineWindow, fSearchFilename);
	wclrtoeol(fInputLineWindow);
}

void FileSelect::ClearSearchFilename()
{
	fSearchFilename[0] = 0;
	fSearchFilenameLength = 0;
	DisplaySearchFilename();
}

DirEntry::EEntryType FileSelect::BuildFilename(char* filename)
{
	strncpy(filename, fPath, PATH_MAX-1);
	filename[PATH_MAX-1] = 0;
	unsigned int len=strlen(filename);
	if ( (len > 0) && (filename[len-1] != DIR_SEPARATOR)) {
		filename[len++] = DIR_SEPARATOR;
		filename[len] = 0;
	}
	if (len < PATH_MAX-1) {
		strncpy(filename + len, fDirectory->Get(fPosition)->fName, PATH_MAX - 1 - len);
	}
	struct stat sb;
	if (stat(filename, &sb) == 0) {
		if (S_ISDIR(sb.st_mode)) {
			return DirEntry::eDirectory;
		}
		if (S_ISREG(sb.st_mode)) {
			return DirEntry::eFile;
		}
	}
	return DirEntry::eUnknown;
}

void FileSelect::SetEnableVirtualDriveKey(bool on)
{
	fEnableVirtualDriveKey = on;
}

bool FileSelect::InputIsVirtualDrive() const
{
	return fInputIsVirtualDrive;
}

void FileSelect::SetEnableDos2xDirectory(bool on)
{
	fEnableDos2xDirectory = on;
}
