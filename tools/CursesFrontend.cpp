/*
   CursesFrontend.cpp - curses frontend for atariserver

   Copyright (C) 2003-2008 Matthias Reichl <hias@horus.com>

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

#include "CursesFrontend.h"
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

#include "OS.h"
#include "FileInput.h"
#include "DeviceManager.h"
#include "SIOTracer.h"
#include "AtariDebug.h"
#include "FileSelect.h"
#include "Version.h"
#include "Dos2xUtils.h"
#include "MiscUtils.h"
#include "CasHandler.h"

CursesFrontend::CursesFrontend(RCPtr<DeviceManager>& manager, bool useColor)
	: fDeviceManager(manager),
	  fTraceLevel(0),
	  fDirCache(new DirectoryCache),
	  fCursorStatus(true),
	  fAuxWindowStatus(true),
	  fCasWindowStatus(true),
	  fGotSigWinCh(false),
	  fAlreadyReportedCursorOnProblem(false),
	  fAlreadyReportedCursorOffProblem(false),
	  fAskBeforeQuit(false)
{
	// init curses

	initscr();
	nonl();
	cbreak();
	noecho();

	int tops, toph, drives, driveh, logts, logth;
	int logs, logh, bots, both, inps, inph;
	int auxs, auxh;

	// create windows
	CalculateWindowPositions(
		tops, toph, drives, driveh, logts, logth,
		logs, logh, bots, both, inps, inph, auxs, auxh);

	fTopLineWindow = newwin(toph, fScreenWidth, tops, 0);
        fDriveStatusWindow = newwin(driveh, fScreenWidth, drives, 0);
        fCasWindow = newwin(driveh, fScreenWidth, drives, 0);
	fStatusLineWindow = newwin(logth, fScreenWidth, logts, 0);
	fLogWindow = newwin(logh, fScreenWidth, logs, 0);
	fBottomLineWindow = newwin(both, fScreenWidth, bots, 0);
	fInputLineWindow = newwin(inph, fScreenWidth, inps, 0);
	fAuxWindow = newwin(auxh, fScreenWidth, auxs, 0);

	/*
	leaveok(fTopLineWindow, TRUE);
	leaveok(fDriveStatusWindow, TRUE);
	leaveok(fStatusLineWindow, TRUE);
	leaveok(fLogWindow, TRUE);
	leaveok(fBottomLineWindow, TRUE);
	leaveok(fAuxWindow, TRUE);
	*/

        fTopLinePanel = new_panel(fTopLineWindow);
	fDriveStatusPanel = new_panel(fDriveStatusWindow);
	fCasPanel = new_panel(fCasWindow);
	fStatusLinePanel = new_panel(fStatusLineWindow);
	fLogPanel = new_panel(fLogWindow);
	fBottomLinePanel = new_panel(fBottomLineWindow);
	fInputLinePanel = new_panel(fInputLineWindow);
	fAuxPanel = new_panel(fAuxWindow);

	ShowAuxWindow(false);
	ShowCasWindow(false);

	// init colors
	if (useColor && has_colors()) {
		start_color();
		init_pair(1, COLOR_WHITE, COLOR_BLUE);
		init_pair(2, COLOR_YELLOW, COLOR_BLUE);
		init_pair(3, COLOR_YELLOW, COLOR_BLACK);
		init_pair(4, COLOR_GREEN, COLOR_BLACK);
		init_pair(5, COLOR_CYAN, COLOR_BLACK);
		init_pair(6, COLOR_RED, COLOR_BLACK);
		init_pair(7, COLOR_YELLOW, COLOR_RED);
		init_pair(8, COLOR_MAGENTA, COLOR_BLACK);
		init_pair(9, COLOR_BLUE, COLOR_BLACK);

		fLogColorStandard = COLOR_PAIR(0);
		fLogColorHighlight = COLOR_PAIR(3) | A_BOLD;
		fLogColorOK = COLOR_PAIR(4);
		fLogColorDebug = COLOR_PAIR(8);
		fLogColorWarning = COLOR_PAIR(5);
		fLogColorError = COLOR_PAIR(6);

		fDriveColorStandard = COLOR_PAIR(0);
		fDriveColorInactive = COLOR_PAIR(9);
		fDriveColorWritable = COLOR_PAIR(4);
		fDriveColorWriteProtected = COLOR_PAIR(6);
		fDriveColorChanged = COLOR_PAIR(6);

		fCasColorStandard = COLOR_PAIR(0);
		fCasColorInfo = COLOR_PAIR(3);

		fPrinterColorSpawned = COLOR_PAIR(4);
		fPrinterColorError = COLOR_PAIR(6);

		fStatusColorStandard = COLOR_PAIR(2) | A_BOLD;

		fAuxColorStandard = COLOR_PAIR(0);
		fAuxColorSelected = COLOR_PAIR(7) | A_BOLD;

		wbkgdset(fTopLineWindow, COLOR_PAIR(2) | A_BOLD | ' ');
		wbkgdset(fBottomLineWindow, COLOR_PAIR(1) | ' ');

	} else {
		fLogColorStandard = 0;
		fLogColorOK = 0;
		fLogColorDebug = 0;
		fLogColorWarning = 0;
		fLogColorError = 0;

		fDriveColorStandard = 0;
		fDriveColorInactive = 0;
		fDriveColorWritable = 0;
		fDriveColorWriteProtected = 0;
		fDriveColorChanged = 0;

		fCasColorStandard = 0;
		fCasColorInfo = 0;

		fPrinterColorSpawned = 0;
		fPrinterColorError = 0;

		fStatusColorStandard = A_REVERSE;

		fAuxColorStandard = 0;
		fAuxColorSelected = A_REVERSE;

		wbkgdset(fTopLineWindow, A_REVERSE | ' ');
		wbkgdset(fBottomLineWindow, A_REVERSE | ' ');
	}

	wbkgdset(fStatusLineWindow, fStatusColorStandard | ' ');
	wbkgdset(fAuxWindow, fAuxColorStandard | ' ');

	scrollok(fLogWindow, TRUE);
	keypad(fInputLineWindow, TRUE);

	InitWindows(false);
	UpdateScreen();
}

CursesFrontend::~CursesFrontend()
{
	del_panel(fTopLinePanel);
	del_panel(fDriveStatusPanel);
	del_panel(fCasPanel);
	del_panel(fStatusLinePanel);
	del_panel(fLogPanel);
	del_panel(fBottomLinePanel);
	del_panel(fInputLinePanel);
	del_panel(fAuxPanel);

	delwin(fTopLineWindow);
	delwin(fDriveStatusWindow);
	delwin(fCasWindow);
	delwin(fStatusLineWindow);
	delwin(fLogWindow);
	delwin(fBottomLineWindow);
	delwin(fInputLineWindow);
	delwin(fAuxWindow);

	update_panels();
	doupdate();
	endwin();
}

bool CursesFrontend::CalculateWindowPositions(
	int& toplineStart, int& toplineHeight,
	int& drivestatusStart, int& drivestatusHeight,
	int& logtitleStart, int& logtitleHeight,
	int& logStart, int& logHeight,
	int& bottomStart, int& bottomHeight,
	int& inputStart, int& inputHeight,
	int& auxStart, int& auxHeight)
{
	getmaxyx(stdscr, fScreenHeight, fScreenWidth);
	DPRINTF("new width: %d height: %d", fScreenWidth, fScreenHeight);

	if (fScreenHeight < 15) {
		return false;
	}

	toplineStart = 0; toplineHeight = 1;

	drivestatusStart = toplineStart + toplineHeight;
	drivestatusHeight = 11;

	logtitleStart = drivestatusStart + drivestatusHeight;
	logtitleHeight = 1;

	inputHeight = 1;
	inputStart = fScreenHeight - inputHeight;

	bottomHeight = 1;
	bottomStart = inputStart - bottomHeight;

	logStart = logtitleStart + logtitleHeight;
	logHeight = bottomStart - logStart;

	auxStart = drivestatusStart;
	auxHeight = bottomStart - auxStart;

	fDriveYStart = 1;
	fDriveNoX = 0;

	fDriveSectorsX = fDriveNoX + 4;
	fDriveDensityX = fDriveSectorsX + 6;

	fDriveProtectedX = fDriveDensityX + 3;
	fDriveChangedX = fDriveProtectedX + 2;

	fDriveFilenameX = fDriveChangedX + 3;
	if (fDriveFilenameX < fScreenWidth) {
		fDriveFilenameLength = fScreenWidth - fDriveFilenameX;
	} else {
		fDriveFilenameLength = 0;
	}

	fCasFilenameY = 1;
	fCasDescriptionY = 2;
	fCasBlockY = 3;
	fCasPartY = 4;
	fCasBaudY = 5;
	fCasGapY = 6;
	fCasLengthY = 7;
	fCasStateY = 9;

	fCasXStart = 7;

	return true;
}

void CursesFrontend::UpdateScreen()
{
	update_panels();
	doupdate();
}

void CursesFrontend::HandleResize(bool redrawDriveStatusOnly)
{
	if (!fGotSigWinCh) {
		return;
	}
	fGotSigWinCh = false;

	//endwin();
	//refresh();

	// try to determine new window size
	struct winsize ws;
	int fd;
	if ( (fd = open("/dev/tty", O_RDONLY)) != -1) {
		if (ioctl(fd, TIOCGWINSZ, &ws) != -1) {
			resizeterm(ws.ws_row, ws.ws_col);
		}
		close(fd);
	}

	int tops, toph, drives, driveh, logts, logth;
	int logs, logh, bots, both, inps, inph;
	int auxs, auxh;

	// resize windows
	if ( CalculateWindowPositions(
		tops, toph, drives, driveh, logts, logth,
		logs, logh, bots, both, inps, inph, auxs, auxh) ) {

		wresize(fTopLineWindow, toph, fScreenWidth);
		wresize(fDriveStatusWindow, driveh, fScreenWidth);
		wresize(fCasWindow, driveh, fScreenWidth);
		wresize(fStatusLineWindow, logth, fScreenWidth);
		wresize(fLogWindow, logh, fScreenWidth);
		wresize(fBottomLineWindow, both, fScreenWidth);
		wresize(fInputLineWindow, inph, fScreenWidth);
		wresize(fAuxWindow, auxh, fScreenWidth);
	
		replace_panel(fTopLinePanel, fTopLineWindow);
		replace_panel(fDriveStatusPanel, fDriveStatusWindow);
		replace_panel(fCasPanel, fCasWindow);
		replace_panel(fStatusLinePanel, fStatusLineWindow);
		replace_panel(fLogPanel, fLogWindow);
		replace_panel(fBottomLinePanel, fBottomLineWindow);
		replace_panel(fInputLinePanel, fInputLineWindow);
		replace_panel(fAuxPanel, fAuxWindow);

		move_panel(fTopLinePanel, tops, 0);
		move_panel(fDriveStatusPanel, drives, 0);
		move_panel(fCasPanel, drives, 0);
		move_panel(fStatusLinePanel, logts, 0);
		move_panel(fLogPanel, logs, 0);
		move_panel(fBottomLinePanel, bots, 0);
		move_panel(fInputLinePanel, inps, 0);
		move_panel(fAuxPanel, auxs, 0);
	}

	InitDriveStatusWindows();
	if (!redrawDriveStatusOnly) {
		InitTopAndBottomLines();
	}

	bool curs = ShowCursor(!fCursorStatus);
	ShowCursor(curs);

	UpdateScreen();
}

void CursesFrontend::InitWindows(bool resetCursor)
{
	InitDriveStatusWindows();
	InitTopAndBottomLines();
	if (resetCursor) {
		bool curs = ShowCursor(!fCursorStatus);
		ShowCursor(curs);
	}
	UpdateScreen();
}

void CursesFrontend::InitDriveStatusWindows()
{
	werase(fDriveStatusWindow);
	werase(fCasWindow);
	werase(fStatusLineWindow);
	werase(fLogWindow);

	fFirstLogLine = true;

	DisplayStatusLine();
	DisplayDriveStatus();
	DisplayPrinterStatus();
}

void CursesFrontend::InitTopAndBottomLines()
{
	werase(fTopLineWindow);
	werase(fBottomLineWindow);
	werase(fInputLineWindow);
	werase(fAuxWindow);
	InitTopLine();
	ShowStandardHint();
}

bool CursesFrontend::ShowAuxWindow(bool on)
{
	bool oldStat = fAuxWindowStatus;
	if (on) {
		show_panel(fAuxPanel);
		top_panel(fAuxPanel);
	} else {
		hide_panel(fAuxPanel);
	}
	top_panel(fInputLinePanel);
	fAuxWindowStatus = on;
	return oldStat;
}

bool CursesFrontend::ShowCasWindow(bool on)
{
	bool oldStat = fCasWindowStatus;
	if (on) {
		show_panel(fCasPanel);
		top_panel(fCasPanel);
	} else {
		hide_panel(fCasPanel);
	}
	top_panel(fInputLinePanel);
	fCasWindowStatus = on;
	return oldStat;
}

void CursesFrontend::InitTopLine()
{
	SetTopLineFilename(0, true);
}

void CursesFrontend::SetTopLineString(const char* string)
{
	werase(fTopLineWindow);
	mvwaddstr(fTopLineWindow, 0, 0, "atariserver " VERSION_STRING " -- ");
	unsigned int y,x;
	getyx(fTopLineWindow, y, x);

	// suppress unused warning
	(void)y;

	if (string && (x < fScreenWidth)) {
		unsigned int len = strlen(string);
		if (x+len > fScreenWidth) {
			len = fScreenWidth - x;
		}
		waddnstr(fTopLineWindow, string, len);
	}
}

void CursesFrontend::SetTopLineFilename(const char* string, bool appendSlash)
{
	werase(fTopLineWindow);
	mvwaddstr(fTopLineWindow, 0, 0, "atariserver " VERSION_STRING " -- ");
	unsigned int y,x;
	getyx(fTopLineWindow, y, x);

	// suppress unused warning
	(void)y;

	if (x < fScreenWidth) {

		char cwd[PATH_MAX];
		if (string == NULL) {
			cwd[0] = 0;
			getcwd(cwd, PATH_MAX-1);
			cwd[PATH_MAX-1] = 0;
			string = cwd;
		}

		char* str;

		unsigned int len = strlen(string);
		if (appendSlash && (len > 0) && (string[len-1] != DIR_SEPARATOR) && (len < PATH_MAX-1)) {
			char* str2 = new char[len+2];
			strcpy(str2, string);
			str2[len++] = DIR_SEPARATOR;
			str2[len] = 0;
			str = MiscUtils::ShortenFilename(str2, fScreenWidth - x);
			delete[] str2;
		} else {
			str = MiscUtils::ShortenFilename(string, fScreenWidth - x);
		}

		if (str) {
			waddstr(fTopLineWindow, str);
			delete[] str;
		}
	}
}

void CursesFrontend::DisplayStatusLine()
{
	wmove(fStatusLineWindow, 0, 0);
	waddstr(fStatusLineWindow, " command: ");

	switch (fDeviceManager->GetSioServerMode()) {
	case SIOWrapper::eCommandLine_RI:
		waddstr(fStatusLineWindow, "RING"); break;
	case SIOWrapper::eCommandLine_DSR:
		waddstr(fStatusLineWindow, "DSR"); break;
	case SIOWrapper::eCommandLine_CTS:
		waddstr(fStatusLineWindow, "CTS"); break;
	case SIOWrapper::eCommandLine_None:
		waddstr(fStatusLineWindow, "None"); break;
	default:
		waddstr(fStatusLineWindow, "???"); break;
	}

	waddstr(fStatusLineWindow, "  speed: ");
	if (fDeviceManager->GetHighSpeedMode()) {
		wprintw(fStatusLineWindow, "high (%d)", fDeviceManager->GetHighSpeedPokeyDivisor());
	} else {
		waddstr(fStatusLineWindow, "low");
	}

	waddstr(fStatusLineWindow, "  timing: ");
	switch (fDeviceManager->GetSioTiming()) {
	case SIOWrapper::eStrictTiming:
		waddstr(fStatusLineWindow, "strict");
		break;
	case SIOWrapper::eRelaxedTiming:
		waddstr(fStatusLineWindow, "relaxed");
		break;
	default:
		waddstr(fStatusLineWindow, "???");
	}

	waddstr(fStatusLineWindow, "  XF551: ");
	if (fDeviceManager->GetXF551Mode()) {
		waddstr(fStatusLineWindow, "on");
	} else {
		waddstr(fStatusLineWindow, "off");
	}

	wprintw(fStatusLineWindow, "  trace: %d", fTraceLevel);

	waddstr(fStatusLineWindow, "  RC: ");

	if (fDeviceManager->DeviceIsActive(DeviceManager::eRemoteControl)) {
		waddstr(fStatusLineWindow, "on");
	} else {
		waddstr(fStatusLineWindow, "off");
	}
	wclrtoeol(fStatusLineWindow);
}

void CursesFrontend::DisplayDriveStatus(DeviceManager::EDriveNumber drive)
{
	if ( (drive == DeviceManager::eAllDrives) || fDeviceManager->DriveNumberOK(drive) ) {
		int min=drive, max=drive;
		if (drive == DeviceManager::eAllDrives) {
			min = DeviceManager::eMinDriveNumber;
			max = DeviceManager::eMaxDriveNumber;
		}
	
		for (int i = min; i<=max; i++) {
			DeviceManager::EDriveNumber d = DeviceManager::EDriveNumber(i);
			DisplayDriveNumber(d);
			DisplayDriveChangedStatus(d);
			DisplayDriveProtectedStatus(d);
			DisplayDriveDensity(d);
			DisplayDriveSectors(d);
			DisplayDriveFilename(d);
		}
	}
}

void CursesFrontend::DisplayDriveNumber(DeviceManager::EDriveNumber drive)
{
	if ( (drive == DeviceManager::eAllDrives) || fDeviceManager->DriveNumberOK(drive) ) {
		int min=drive, max=drive;
		if (drive == DeviceManager::eAllDrives) {
			min = DeviceManager::eMinDriveNumber;
			max = DeviceManager::eMaxDriveNumber;
		}
	
		for (int i = min; i<=max; i++) {
			DeviceManager::EDriveNumber d = DeviceManager::EDriveNumber(i);
			if (fDeviceManager->DriveInUse(d) && !fDeviceManager->DeviceIsActive(d)) {
				wattrset(fDriveStatusWindow, fDriveColorInactive);
			}
			wmove(fDriveStatusWindow, fDriveYStart + d - DeviceManager::eMinDriveNumber, fDriveNoX);
			wprintw(fDriveStatusWindow,"D%d:", d);
			wattrset(fDriveStatusWindow, fDriveColorStandard);
		}
	}
}

void CursesFrontend::DisplayDriveChangedStatus(DeviceManager::EDriveNumber drive)
{
	if ( (drive == DeviceManager::eAllDrives) || fDeviceManager->DriveNumberOK(drive) ) {
		int min=drive, max=drive;
		if (drive == DeviceManager::eAllDrives) {
			min = DeviceManager::eMinDriveNumber;
			max = DeviceManager::eMaxDriveNumber;
		}
	
		for (int i = min; i<=max; i++) {
			DeviceManager::EDriveNumber d = DeviceManager::EDriveNumber(i);
			wmove(fDriveStatusWindow, fDriveYStart + d - DeviceManager::eMinDriveNumber, fDriveChangedX);
			if (fDeviceManager->DriveInUse(d)) {
				if (fDeviceManager->DeviceIsActive(d)) {
					if (fDeviceManager->DriveIsVirtualImage(d)) {
						wattrset(fDriveStatusWindow, fDriveColorChanged);
						waddch(fDriveStatusWindow, 'V');
						wattrset(fDriveStatusWindow, fDriveColorStandard);
					} else if (fDeviceManager->DriveIsChanged(d)) {
						wattrset(fDriveStatusWindow, fDriveColorChanged);
						waddch(fDriveStatusWindow, 'C');
						wattrset(fDriveStatusWindow, fDriveColorStandard);
					} else {
						waddch(fDriveStatusWindow, ' ');
					}
				} else {
					wattrset(fDriveStatusWindow, fDriveColorInactive);
					waddch(fDriveStatusWindow, 'I');
					wattrset(fDriveStatusWindow, fDriveColorStandard);
				}
			} else {
				waddch(fDriveStatusWindow, '-');
			}
		}
	}
}

void CursesFrontend::DisplayDriveProtectedStatus(DeviceManager::EDriveNumber drive)
{
	if ( (drive == DeviceManager::eAllDrives) || fDeviceManager->DriveNumberOK(drive) ) {
		int min=drive, max=drive;
		if (drive == DeviceManager::eAllDrives) {
			min = DeviceManager::eMinDriveNumber;
			max = DeviceManager::eMaxDriveNumber;
		}
	
		for (int i = min; i<=max; i++) {
			DeviceManager::EDriveNumber d = DeviceManager::EDriveNumber(i);
			wmove(fDriveStatusWindow, fDriveYStart + d - DeviceManager::eMinDriveNumber, fDriveProtectedX);
			if (fDeviceManager->DriveInUse(d)) {
				bool act = fDeviceManager->DeviceIsActive(d);
				if (fDeviceManager->DriveIsWriteProtected(d)) {
					if (act) {
						wattrset(fDriveStatusWindow, fDriveColorWriteProtected);
					} else {
						wattrset(fDriveStatusWindow, fDriveColorInactive);
					}
					waddch(fDriveStatusWindow, 'P');
					wattrset(fDriveStatusWindow, fDriveColorStandard);
				} else {
					if (act) {
						wattrset(fDriveStatusWindow, fDriveColorWritable);
					} else {
						wattrset(fDriveStatusWindow, fDriveColorInactive);
					}
					waddch(fDriveStatusWindow, 'W');
					wattrset(fDriveStatusWindow, fDriveColorStandard);
				}
			} else {
				waddch(fDriveStatusWindow, '-');
			}
		}
	}
}

void CursesFrontend::DisplayDriveDensity(DeviceManager::EDriveNumber drive)
{
	if ( (drive == DeviceManager::eAllDrives) || fDeviceManager->DriveNumberOK(drive) ) {
		int min=drive, max=drive;
		if (drive == DeviceManager::eAllDrives) {
			min = DeviceManager::eMinDriveNumber;
			max = DeviceManager::eMaxDriveNumber;
		}
	
		for (int i = min; i<=max; i++) {
			DeviceManager::EDriveNumber d = DeviceManager::EDriveNumber(i);
			wmove(fDriveStatusWindow, fDriveYStart + d - DeviceManager::eMinDriveNumber, fDriveDensityX);
			if (fDeviceManager->DriveInUse(d)) {
				if (!fDeviceManager->DeviceIsActive(d)) {
					wattrset(fDriveStatusWindow, fDriveColorInactive);
				}
				switch (fDeviceManager->GetConstDiskImage(d)->GetSectorLength()) {
				case e128BytesPerSector:
					waddch(fDriveStatusWindow, 'S');
					break;
				case e256BytesPerSector:
					waddch(fDriveStatusWindow, 'D');
					break;
				case e512BytesPerSector:
					waddch(fDriveStatusWindow, 'Q');
					break;
				case e1kPerSector:
					waddch(fDriveStatusWindow, '1');
					break;
				case e2kPerSector:
					waddch(fDriveStatusWindow, '2');
					break;
				case e4kPerSector:
					waddch(fDriveStatusWindow, '4');
					break;
				case e8kPerSector:
					waddch(fDriveStatusWindow, '8');
					break;
				default:
					waddch(fDriveStatusWindow, '?');
					break;
				}
				wattrset(fDriveStatusWindow, fDriveColorStandard);
			} else {
				waddch(fDriveStatusWindow, '-');
			}
		}
	}
}

void CursesFrontend::DisplayDriveSectors(DeviceManager::EDriveNumber drive)
{
	if ( (drive == DeviceManager::eAllDrives) || fDeviceManager->DriveNumberOK(drive) ) {
		int min=drive, max=drive;
		if (drive == DeviceManager::eAllDrives) {
			min = DeviceManager::eMinDriveNumber;
			max = DeviceManager::eMaxDriveNumber;
		}
	
		for (int i = min; i<=max; i++) {
			DeviceManager::EDriveNumber d = DeviceManager::EDriveNumber(i);
			wmove(fDriveStatusWindow, fDriveYStart + d - DeviceManager::eMinDriveNumber, fDriveSectorsX);
			if (fDeviceManager->DriveInUse(drive)) {
				if (!fDeviceManager->DeviceIsActive(d)) {
					wattrset(fDriveStatusWindow, fDriveColorInactive);
				}
				wprintw(fDriveStatusWindow, "%5d", fDeviceManager->GetConstDiskImage(d)->GetNumberOfSectors());
				wattrset(fDriveStatusWindow, fDriveColorStandard);
			} else {
				waddstr(fDriveStatusWindow, "-----");
			}
		}
	}
}

void CursesFrontend::DisplayDriveFilename(DeviceManager::EDriveNumber drive)
{
	if ( (drive == DeviceManager::eAllDrives) || fDeviceManager->DriveNumberOK(drive) ) {
		int min=drive, max=drive;
		if (drive == DeviceManager::eAllDrives) {
			min = DeviceManager::eMinDriveNumber;
			max = DeviceManager::eMaxDriveNumber;
		}
	
		for (int i = min; i<=max; i++) {
			DeviceManager::EDriveNumber d = DeviceManager::EDriveNumber(i);
			unsigned int len=0;
			const char* filename;

			if (fDeviceManager->DriveInUse(d)) {
				filename = fDeviceManager->GetImageFilename(d);
				if (!fDeviceManager->DeviceIsActive(d)) {
					wattrset(fDriveStatusWindow, fDriveColorInactive);
				}
			} else {
				filename = "<empty>";
			}

			wmove(fDriveStatusWindow, fDriveYStart + d - DeviceManager::eMinDriveNumber, fDriveFilenameX);
	
			if (filename) {
				char *sf = MiscUtils::ShortenFilename(filename, fDriveFilenameLength);
				if (sf) {
					len = strlen(sf);
					waddstr(fDriveStatusWindow, sf);
					delete[] sf;
				} else {
					len = 0;
				}
			}
			if (len < fDriveFilenameLength) {
				wclrtoeol(fDriveStatusWindow);
			}
			wattrset(fDriveStatusWindow, fDriveColorStandard);
		}
	}
}

void CursesFrontend::DisplayPrinterStatus()
{
	DisplayPrinterNumber();
	DisplayPrinterEOLConversion();
	DisplayPrinterRunningStatus();
	DisplayPrinterFilename();
}

void CursesFrontend::DisplayPrinterNumber()
{
	if (fDeviceManager->DriveInUse(DeviceManager::ePrinter) && !fDeviceManager->DeviceIsActive(DeviceManager::ePrinter)) {
		wattrset(fDriveStatusWindow, fDriveColorInactive);
	}
	wmove(fDriveStatusWindow, fDriveYStart + DeviceManager::ePrinter - DeviceManager::eMinDriveNumber, fDriveNoX);
	wprintw(fDriveStatusWindow,"P1:");
	wattrset(fDriveStatusWindow, fDriveColorStandard);
}

void CursesFrontend::DisplayPrinterEOLConversion()
{
	wmove(fDriveStatusWindow, fDriveYStart + DeviceManager::ePrinter - DeviceManager::eMinDriveNumber, fDriveSectorsX);
	if (fDeviceManager->DriveInUse(DeviceManager::ePrinter)) {
		if (!fDeviceManager->DeviceIsActive(DeviceManager::ePrinter)) {
			wattrset(fDriveStatusWindow, fDriveColorInactive);
		}
		switch (fDeviceManager->GetPrinterEOLConversion()) {
		case PrinterHandler::eRaw:
			wprintw(fDriveStatusWindow, "  raw"); break;
		case PrinterHandler::eLF:
			wprintw(fDriveStatusWindow, "   LF"); break;
		case PrinterHandler::eCR:
			wprintw(fDriveStatusWindow, "   CR"); break;
		case PrinterHandler::eCRLF:
			wprintw(fDriveStatusWindow, "CR+LF"); break;
		};
	} else {
		waddstr(fDriveStatusWindow, "-----");
	}
	wmove(fDriveStatusWindow, fDriveYStart + DeviceManager::ePrinter - DeviceManager::eMinDriveNumber, fDriveDensityX);
	waddstr(fDriveStatusWindow, "-");
	wmove(fDriveStatusWindow, fDriveYStart + DeviceManager::ePrinter - DeviceManager::eMinDriveNumber, fDriveProtectedX);
	waddstr(fDriveStatusWindow, "-");
	wattrset(fDriveStatusWindow, fDriveColorStandard);
}

void CursesFrontend::DisplayPrinterRunningStatus()
{
	wmove(fDriveStatusWindow, fDriveYStart + DeviceManager::ePrinter - DeviceManager::eMinDriveNumber, fDriveChangedX);
	if (fDeviceManager->DriveInUse(DeviceManager::ePrinter)) {
		if (fDeviceManager->DeviceIsActive(DeviceManager::ePrinter)) {
			switch (fDeviceManager->GetPrinterRunningStatus()) {
			case PrinterHandler::eStatusOK:
				waddch(fDriveStatusWindow, ' ');
				break;
			case PrinterHandler::eStatusSpawned:
				wattrset(fDriveStatusWindow, fPrinterColorSpawned);
				waddch(fDriveStatusWindow, 'S');
				wattrset(fDriveStatusWindow, fDriveColorStandard);
				break;
			case PrinterHandler::eStatusError:
				wattrset(fDriveStatusWindow, fPrinterColorError);
				waddch(fDriveStatusWindow, 'E');
				wattrset(fDriveStatusWindow, fDriveColorStandard);
				break;
			}
		} else {
			wattrset(fDriveStatusWindow, fDriveColorInactive);
			waddch(fDriveStatusWindow, 'I');
			wattrset(fDriveStatusWindow, fDriveColorStandard);
		}
	} else {
		waddch(fDriveStatusWindow, '-');
	}
}

void CursesFrontend::DisplayPrinterFilename()
{
	unsigned int len=0;
	const char* filename;

	if (fDeviceManager->DriveInUse(DeviceManager::ePrinter)) {
		filename = fDeviceManager->GetPrinterFilename();
		if (!fDeviceManager->DeviceIsActive(DeviceManager::ePrinter)) {
			wattrset(fDriveStatusWindow, fDriveColorInactive);
		}
	} else {
		filename = "<empty>";
	}

	wmove(fDriveStatusWindow, fDriveYStart + DeviceManager::ePrinter - DeviceManager::eMinDriveNumber, fDriveFilenameX);

	if (filename) {
		char *sf = MiscUtils::ShortenFilename(filename, fDriveFilenameLength);
		if (sf) {
			len = strlen(sf);
			waddstr(fDriveStatusWindow, sf);
			delete[] sf;
		} else {
			len = 0;
		}
	}
	if (len < fDriveFilenameLength) {
		wclrtoeol(fDriveStatusWindow);
	}
	wattrset(fDriveStatusWindow, fDriveColorStandard);
}
	

void CursesFrontend::DisplayCasStatus()
{
	DisplayCasText();
	DisplayCasFilename();
	DisplayCasDescription();
	DisplayCasBlock();
	DisplayCasState();
	UpdateScreen();
}

void CursesFrontend::DisplayCasText()
{
	wmove(fCasWindow, fCasFilenameY, 0);
	wattrset(fCasWindow, fCasColorStandard);
	waddstr(fCasWindow,"    C:");
	
	wmove(fCasWindow, fCasDescriptionY, 0);
	waddstr(fCasWindow," Desc:");
	
	wmove(fCasWindow, fCasBlockY, 0);
	waddstr(fCasWindow,"Block:");
	
	wmove(fCasWindow, fCasPartY, 0);
	waddstr(fCasWindow," Part:");
	
	wmove(fCasWindow, fCasBaudY, 0);
	waddstr(fCasWindow," Baud:");
	
	wmove(fCasWindow, fCasGapY, 0);
	waddstr(fCasWindow,"  Gap:");
	
	wmove(fCasWindow, fCasLengthY, 0);
	waddstr(fCasWindow,"Bytes:");

	wmove(fCasWindow, fCasStateY, 0);
	waddstr(fCasWindow,"State:");
}


void CursesFrontend::DisplayCasFilename()
{
	unsigned int len=0;
	const char* filename = 0;

	RCPtr<CasHandler> casHandler = fDeviceManager->GetCasHandler();

	if (!casHandler) {
		Assert(false);
		return;
	}

	filename = casHandler->GetFilename();

	wmove(fCasWindow, fCasFilenameY, fCasXStart);
	wattrset(fCasWindow, fCasColorStandard);

	if (filename) {
		char *sf = MiscUtils::ShortenFilename(filename, fScreenWidth - fCasXStart);
		if (sf) {
			len = strlen(sf);
			waddstr(fCasWindow, sf);
			delete[] sf;
		} else {
			len = 0;
		}
	}
	if (len < fScreenWidth - fCasXStart) {
		wclrtoeol(fCasWindow);
	}
}

void CursesFrontend::DisplayCasDescription()
{
	unsigned int len=0;
	const char* desc = 0;

	RCPtr<CasHandler> casHandler = fDeviceManager->GetCasHandler();

	if (!casHandler) {
		Assert(false);
		return;
	}

	desc = casHandler->GetDescription();

	wmove(fCasWindow, fCasDescriptionY, fCasXStart);
	wattrset(fCasWindow, fCasColorStandard);

	if (desc) {
		char *sf = MiscUtils::ShortenFilename(desc, fScreenWidth - fCasXStart);
		if (sf) {
			len = strlen(sf);
			waddstr(fCasWindow, sf);
			delete[] sf;
		} else {
			len = 0;
		}
	}
	if (len < fScreenWidth - fCasXStart) {
		wclrtoeol(fCasWindow);
	}
}


void CursesFrontend::DisplayCasBlock()
{
	RCPtr<CasHandler> casHandler = fDeviceManager->GetCasHandler();

	if (!casHandler) {
		Assert(false);
		return;
	}

	wmove(fCasWindow, fCasBlockY, fCasXStart);
	wattrset(fCasWindow, fCasColorInfo);

	wprintw(fCasWindow, "%5d", casHandler->GetCurrentBlockNumber() + 1);

	wattrset(fCasWindow, fCasColorStandard);
	wprintw(fCasWindow, " of %d", casHandler->GetNumberOfBlocks());

	wmove(fCasWindow, fCasPartY, fCasXStart);
	wattrset(fCasWindow, fCasColorInfo);

	wprintw(fCasWindow, "%5d", casHandler->GetCurrentPartNumber() + 1);

	wattrset(fCasWindow, fCasColorStandard);
	wprintw(fCasWindow, " of %d", casHandler->GetNumberOfParts());

	wmove(fCasWindow, fCasBaudY, fCasXStart);
	wattrset(fCasWindow, fCasColorInfo);

	if (casHandler->CurrentBlockIsDataBlock()) {
		wprintw(fCasWindow, "%5d      ",
			casHandler->GetCurrentBlockBaudRate());
	} else {
		waddstr(fCasWindow,"  n/a (fsk)");
	}

	wmove(fCasWindow, fCasGapY, fCasXStart);
	wprintw(fCasWindow, "%5d",
		casHandler->GetCurrentBlockGap());

	wmove(fCasWindow, fCasLengthY, fCasXStart);

	unsigned int blocklen = casHandler->GetCurrentBlockLength();
	if (casHandler->CurrentBlockIsFskBlock()) {
		blocklen *= 2;
	}
	wprintw(fCasWindow, "%5d", blocklen);

	wattrset(fCasWindow, fCasColorStandard);
}

void CursesFrontend::DisplayCasState()
{
	RCPtr<CasHandler> casHandler = fDeviceManager->GetCasHandler();

	if (!casHandler) {
		Assert(false);
		return;
	}

	wmove(fCasWindow, fCasStateY, fCasXStart);

	wattrset(fCasWindow, fCasColorInfo);

	switch (casHandler->GetState()) {
	case CasHandler::eStatePaused:
		waddstr(fCasWindow, "Pause      ");
		break;
	case CasHandler::eStateGap:
		waddstr(fCasWindow, "Play Gap   ");
		break;
	case CasHandler::eStatePlaying:
		{
			unsigned int blocklen = casHandler->GetCurrentBlockLength();
			if (blocklen <= CasHandler::eMaxTransferSize) {
				waddstr(fCasWindow, "Play       ");
			} else {
				wprintw(fCasWindow, "Play %d    ", casHandler->GetCurrentBytePos());
			}
		}
		break;
	case CasHandler::eStateDone:
		waddstr(fCasWindow, "Done       ");
		break;
	}

	wattrset(fCasWindow, fCasColorStandard);
}

bool CursesFrontend::ShowCursor(bool on)
{
	bool oldStat = fCursorStatus;
	//ALOG("show cursor: %d old %d", on ? 1 : 0, oldStat ? 1 : 0);
	if (on) {
		if (curs_set(1) == ERR) {
			if (!fAlreadyReportedCursorOnProblem) {
				DPRINTF("enabling cursor failed");
				fAlreadyReportedCursorOnProblem = true;
			}
		}
	} else {
		if (curs_set(0) == ERR) {
			if (!fAlreadyReportedCursorOffProblem) {
				DPRINTF("disabling cursor failed");
				fAlreadyReportedCursorOffProblem = true;
			}
		}
	}
	fCursorStatus = on;
	return oldStat;
}

void CursesFrontend::AddLogLine(const char* str)
{
	StartLogLine();
	AddLogString(str);
	EndLogLine();
}

void CursesFrontend::StartLogLine()
{
	if (!fFirstLogLine) {
		wprintw(fLogWindow,"\n");
	}
	fFirstLogLine = false;
}

void CursesFrontend::EndLogLine()
{
}

void CursesFrontend::AddLogString(const char* str)
{
	wattrset(fLogWindow, fLogColorStandard);
	wprintw(fLogWindow, "%s", str);
}

void CursesFrontend::AddLogHighlightString(const char* str)
{
	wattrset(fLogWindow, fLogColorHighlight);
	wprintw(fLogWindow, "%s", str);
	wattrset(fLogWindow, fLogColorStandard);
}

void CursesFrontend::AddLogOKString(const char* str)
{
	wattrset(fLogWindow, fLogColorOK);
	wprintw(fLogWindow, "%s", str);
	wattrset(fLogWindow, fLogColorStandard);
}

void CursesFrontend::AddLogDebugString(const char* str)
{
	wattrset(fLogWindow, fLogColorDebug);
	wprintw(fLogWindow, "%s", str);
	wattrset(fLogWindow, fLogColorStandard);
}

void CursesFrontend::AddLogWarningString(const char* str)
{
	wattrset(fLogWindow, fLogColorWarning);
	wprintw(fLogWindow, "%s", str);
	wattrset(fLogWindow, fLogColorStandard);
}

void CursesFrontend::AddLogErrorString(const char* str)
{
	wattrset(fLogWindow, fLogColorError);
	wprintw(fLogWindow, "%s", str);
	wattrset(fLogWindow, fLogColorStandard);
}

void CursesFrontend::PrintLogLine(const char* format, ...)
{
	va_list arg;
	va_start(arg, format);

	if (!fFirstLogLine) {
		waddstr(fLogWindow, "\n");
	}
	vw_printw(fLogWindow, format, arg);
	va_end(arg);

	fFirstLogLine = false;
}

int CursesFrontend::GetCh(bool ignoreResize)
{
	int ret;
	int ch;
	do {
		ret = fDeviceManager->DoServing(STDIN_FILENO);

		if (ret == 1 && GotSigWinCh() && !ignoreResize ) {
			ch = KEY_RESIZE;
			break;
		} else if (ret == 0) {
			ch = wgetch(fInputLineWindow);
			if (ch == KEY_RESIZE) {
				//DPRINTF("got KEY_RESIZE from ncurses!");
			} else {
				break;
			}
		}
	} while (1);
	return ch;
}

bool CursesFrontend::IsAbortChar(int ch)
{
	return (ch == ERR) || (ch == 27) || (ch == 7)
		|| (ch == 'q') || (ch == 'Q');
}

DeviceManager::EDriveNumber CursesFrontend::InputDriveNumber(EDriveInputType type)
{
	do {
		int ch = GetCh(true);
		if ( IsAbortChar(ch) ) {
			return DeviceManager::eNoDrive;
		}
		if ( (ch >= '0' + DeviceManager::eMinDriveNumber) &&
		     (ch <= '0' + DeviceManager::eMaxDriveNumber) ) {
			return DeviceManager::EDriveNumber(ch - '0');
		}
		switch (type) {
		case eDriveInputAllPrinterRC:
			if ( (ch == 'p') || (ch == 'P')) {
				return DeviceManager::ePrinter;
			}
			if ( (ch == 'r') || (ch == 'R')) {
				return DeviceManager::eRemoteControl;
			}
			// fallthrough
		case eDriveInputAll:
			if ((ch == 'a') || (ch == 'A')) {
				return DeviceManager::eAllDrives;
			}
		case eDriveInputStandardPlusCassette:
			if ((ch == 'c') || (ch == 'C')) {
				return DeviceManager::eCassette;
			}
		default:
			break;
		}
		beep();
	} while (1);
}

DeviceManager::EDriveNumber CursesFrontend::InputUsedDriveNumber(EDriveInputType type)
{
	do {
		DeviceManager::EDriveNumber d = InputDriveNumber(type);
		if ( (d == DeviceManager::eNoDrive) || (d == DeviceManager::eAllDrives) ) {
			return d;
		}
		if ( fDeviceManager->DriveInUse(d) ) {
			return d;
		}
		beep();
	} while (1);
}

DeviceManager::EDriveNumber CursesFrontend::InputVirtualDriveNumber(EDriveInputType type)
{
	do {
		DeviceManager::EDriveNumber d = InputDriveNumber(type);
		if ( (d == DeviceManager::eNoDrive) || (d == DeviceManager::eAllDrives) ) {
			return d;
		}
		if ( fDeviceManager->DriveInUse(d) && fDeviceManager->DriveIsVirtualImage(d) ) {
			return d;
		}
		beep();
	} while (1);
}

DeviceManager::EDriveNumber CursesFrontend::InputUnusedDriveNumber(EDriveInputType type)
{
	do {
		DeviceManager::EDriveNumber d = InputDriveNumber(type);
		if ( (d == DeviceManager::eNoDrive) || (d == DeviceManager::eAllDrives) ) {
			return d;
		}
		if ( ! fDeviceManager->DriveInUse(d) ) {
			return d;
		}
		beep();
	} while (1);
}

DeviceManager::EDriveNumber CursesFrontend::InputUnusedOrUnloadableDriveNumber(EDriveInputType type)
{
	do {
		DeviceManager::EDriveNumber d = InputDriveNumber(type);
		if ( (d == DeviceManager::eNoDrive) || (d == DeviceManager::eAllDrives) ) {
			return d;
		}
		if ( fDeviceManager->DriveInUse(d) ) {
			if (fDeviceManager->DriveIsVirtualImage(d)) {
				AWARN("D%d: is a virtual drive, please unload it first", d);
			} else if (fDeviceManager->DriveIsChanged(d)) {
				AWARN("D%d: is changed, please unload it first", d);
			} else {
				return d;
			}
		} else {
			return d;
		}
		beep();
	} while (1);
}

CursesFrontend::EYesNo CursesFrontend::InputYesNo()
{
	int ch;
	do {
		ch = GetCh(true);
		if (IsAbortChar(ch)) {
			return eYN_Abort;
		}
		if ( (ch == 'y') || (ch == 'Y') ) {
			return eYN_Yes;
		}
		if ( (ch == 'n') || (ch == 'N') ) {
			return eYN_No;
		}
		beep();
	} while(1);
}

void CursesFrontend::SetTraceLevel(int level)
{
	fTraceLevel = level;
        SIOTracer* tracer = SIOTracer::GetInstance();
	tracer->SetTraceGroup(SIOTracer::eTraceVerboseCommands, level == 1 );
	tracer->SetTraceGroup(SIOTracer::eTracePrinter, level >= 1 );
	tracer->SetTraceGroup(SIOTracer::eTraceCommands, level >= 2 );
	tracer->SetTraceGroup(SIOTracer::eTraceUnhandeledCommands, level >= 2 );
	tracer->SetTraceGroup(SIOTracer::eTraceAtpInfo, level >= 2 );
	tracer->SetTraceGroup(SIOTracer::eTraceDataBlocks, level >= 3 );
}

void CursesFrontend::ShowStandardHint()
{
	werase(fBottomLineWindow);
	wmove(fBottomLineWindow, 0, 0);
	waddstr(fBottomLineWindow,"press 'h' for help, 'q' to quit");
}

void CursesFrontend::ShowYesNoHint()
{
	werase(fBottomLineWindow);
	wmove(fBottomLineWindow, 0, 0);
	waddstr(fBottomLineWindow, "'y'=yes, 'n'=no, '^G','q'=abort");
}

void CursesFrontend::ShowPagerHint()
{
	werase(fBottomLineWindow);
	wmove(fBottomLineWindow, 0, 0);
	waddstr(fBottomLineWindow,"press <space> to continue, '^G','q' to abort");
}

void CursesFrontend::ShowDriveInputHint(EDriveInputHintType type)
{
	werase(fBottomLineWindow);
	wmove(fBottomLineWindow, 0, 0);
	wprintw(fBottomLineWindow,"drive '%d'..'%d'",
		DeviceManager::eMinDriveNumber, DeviceManager::eMaxDriveNumber);
	switch (type) {
	case eDriveInputHintAll:
		waddstr(fBottomLineWindow,", 'a'=all drives");
		break;
	case eDriveInputHintAllChanged:
		waddstr(fBottomLineWindow,", 'a'=all changed drives");
		break;
	case eDriveInputHintAllPrinterRC:
		waddstr(fBottomLineWindow,", 'a'=all drives 'p'=printer 'r'=remote control");
		break;
	case eDriveInputHintStandardPlusCassette:
		waddstr(fBottomLineWindow,", 'c'=cassette");
		break;
	default:
		break;
	}
	waddstr(fBottomLineWindow,", '^G','q'=abort");
}

void CursesFrontend::ShowHint(const char* string)
{
	werase(fBottomLineWindow);
	wmove(fBottomLineWindow, 0, 0);
	wprintw(fBottomLineWindow,"%s", string);
}

void CursesFrontend::ShowFileInputHint(bool enableVirtualDrive)
{
	werase(fBottomLineWindow);
	wmove(fBottomLineWindow, 0, 0);
	wprintw(fBottomLineWindow,"enter filename  <tab>=complete, '^F'=file-select, ");
	if (enableVirtualDrive) {
		wprintw(fBottomLineWindow,"'^V'=virtual drv, ");
	}
	wprintw(fBottomLineWindow,"'^G'=abort");
}

void CursesFrontend::ShowTraceLevelHint()
{
	werase(fBottomLineWindow);
	wmove(fBottomLineWindow, 0, 0);
	waddstr(fBottomLineWindow,"level '0'..'3', '^G','q'=abort");
}

void CursesFrontend::ShowCreateDriveHint(bool enableQD)
{
	werase(fBottomLineWindow);
	wmove(fBottomLineWindow, 0, 0);
	if (enableQD) {
		waddstr(fBottomLineWindow,"'1'=90k '2'=130k '3'=180k '4'=360k 's'=SD 'd'=DD 'f'=QD '^G','q'=abort");
	} else {
		waddstr(fBottomLineWindow,"'1'=90k '2'=130k '3'=180k '4'=360k 's'=SD 'd'=DD '^G','q'=abort");
	}
}

void CursesFrontend::ShowImageSizeHint(int minimumSectors)
{
	werase(fBottomLineWindow);
	wmove(fBottomLineWindow, 0, 0);
	wprintw(fBottomLineWindow,"'%d'..'65535' sectors, '^G'=abort", minimumSectors);
}

void CursesFrontend::ShowSioTimingHint()
{
	werase(fBottomLineWindow);
	wmove(fBottomLineWindow, 0, 0);
	waddstr(fBottomLineWindow,"'s'=strict 'r'=relaxed'^G'=abort");
}

void CursesFrontend::ShowHighSpeedParametersHint()
{
	werase(fBottomLineWindow);
	wmove(fBottomLineWindow, 0, 0);
	waddstr(fBottomLineWindow,"'0'..'63'[',baudrate'] '^G'=abort");
}

void CursesFrontend::ShowPrinterEOLHint()
{
	werase(fBottomLineWindow);
	wmove(fBottomLineWindow, 0, 0);
	waddstr(fBottomLineWindow, "EOL conversion: 'r'=raw, 'l'=LF, 'c'=CR, 'b'=CR+LF, '^G','q'=abort");
}

void CursesFrontend::ShowPrinterFileHint()
{
	werase(fBottomLineWindow);
	wmove(fBottomLineWindow, 0, 0);
	wprintw(fBottomLineWindow,"enter filename or |command  <tab>=complete, '^F'=file-select, ");
	wprintw(fBottomLineWindow,"'^G'=abort");
}

void CursesFrontend::ShowCasHint()
{
	werase(fBottomLineWindow);
	wmove(fBottomLineWindow, 0, 0);
	wprintw(fBottomLineWindow,"use '<space>' to play/pause, cursor keys to seek, '^G'/'q'=abort");
}

void CursesFrontend::AbortInput()
{
	ShowCursor(false);
	ShowStandardHint();
	InitTopLine();
	werase(fInputLineWindow);
	UpdateScreen();
}

void CursesFrontend::ClearInputLine()
{
	werase(fInputLineWindow);
	wmove(fInputLineWindow, 0, 0);
}

void CursesFrontend::ShowDriveNumber(DeviceManager::EDriveNumber d, bool allChanged)
{
	switch (d) {
	case DeviceManager::eNoDrive:
		waddstr(fInputLineWindow, "<aborted>");
		break;
	case DeviceManager::eAllDrives:
		waddstr(fInputLineWindow, "all");
		if (allChanged) {
			waddstr(fInputLineWindow, " changed");
		}
		break;
	case DeviceManager::ePrinter:
		waddstr(fInputLineWindow, "printer");
		break;
	case DeviceManager::eRemoteControl:
		waddstr(fInputLineWindow, "remote control");
		break;
	case DeviceManager::eCassette:
		waddstr(fInputLineWindow, "cassette");
		break;
	default:
		wprintw(fInputLineWindow, "%d", d);
		break;
	}
}

void CursesFrontend::ShowYesNo(EYesNo yn)
{
	switch (yn) {
	case eYN_Abort:
		waddstr(fInputLineWindow, "<aborted>");
		break;
	case eYN_Yes:
		waddstr(fInputLineWindow, "yes");
		break;
	case eYN_No:
		waddstr(fInputLineWindow, "no");
		break;
	}
}

void CursesFrontend::InternalProcessWriteProtectDrive(bool protect)
{
	ShowDriveInputHint(eDriveInputHintAll);
	ClearInputLine();
	if (protect) {
		waddstr(fInputLineWindow, "write protect disk: ");
	} else {
		waddstr(fInputLineWindow, "un-protect disk: ");
	}
	ShowCursor(true);
	UpdateScreen();

	DeviceManager::EDriveNumber d = InputUsedDriveNumber(eDriveInputAll);

	ShowCursor(false);

	if (d == DeviceManager::eNoDrive) {
		AbortInput();
		return;
	}

	ShowDriveNumber(d);

	fDeviceManager->SetWriteProtectImage(d, protect);
	DisplayDriveProtectedStatus(d);
	ShowStandardHint();
	UpdateScreen();
}

void CursesFrontend::ProcessWriteProtectDrive()
{
	InternalProcessWriteProtectDrive(true);
}

void CursesFrontend::ProcessUnprotectDrive()
{
	InternalProcessWriteProtectDrive(false);
}

void CursesFrontend::InternalProcessActivateDrive(bool act)
{
	ShowDriveInputHint(eDriveInputHintAllPrinterRC);
	ClearInputLine();
	if (act) {
		waddstr(fInputLineWindow, "activate disk: ");
	} else {
		waddstr(fInputLineWindow, "de-activate disk: ");
	}
	ShowCursor(true);
	UpdateScreen();

	DeviceManager::EDriveNumber d = InputUsedDriveNumber(eDriveInputAllPrinterRC);

	ShowCursor(false);

	if (d == DeviceManager::eNoDrive) {
		AbortInput();
		return;
	}

	ShowDriveNumber(d);

	fDeviceManager->SetDeviceActive(d, act);
	switch (d) {
	case DeviceManager::ePrinter:
		DisplayPrinterStatus(); break;
	case DeviceManager::eRemoteControl:
		DisplayStatusLine(); break;
	default:
		DisplayDriveStatus(d); break;
	}
	UpdateScreen();
}

void CursesFrontend::ProcessActivateDrive()
{
	InternalProcessActivateDrive(true);
}

void CursesFrontend::ProcessDeactivateDrive()
{
	InternalProcessActivateDrive(false);
}

bool CursesFrontend::ProcessQuit()
{
	bool ret = true;
	bool changedDrives = fDeviceManager->CheckForChangedImages();
	if (changedDrives || fAskBeforeQuit) {
		ShowYesNoHint();
		ClearInputLine();
		if (changedDrives) {
			waddstr(fInputLineWindow, "there are changed images - really quit? ");
		} else {
			waddstr(fInputLineWindow, "really quit? ");
		}
		ShowCursor(true);
		UpdateScreen();

		EYesNo yn = InputYesNo();

		switch (yn) {
		case eYN_Abort:
			AbortInput();
			return false;
		case eYN_Yes:
			ShowYesNo(yn);
			ret = true;
			break;
		case eYN_No:
			ShowYesNo(yn);
			ret = false;
			break;
		}

		ShowCursor(false);
		DisplayStatusLine();
		ShowStandardHint();
		UpdateScreen();
	}

	return ret;
}

void CursesFrontend::ProcessExchangeDrives()
{
	ShowDriveInputHint(eDriveInputHintStandard);
	ClearInputLine();
	waddstr(fInputLineWindow, "exchange drive: ");
	ShowCursor(true);
	UpdateScreen();

	DeviceManager::EDriveNumber d1 = InputDriveNumber(eDriveInputStandard);

	if (d1 == DeviceManager::eNoDrive) {
		AbortInput();
		return;
	}

	wprintw(fInputLineWindow, "%d with drive: ", d1);
	UpdateScreen();

	DeviceManager::EDriveNumber d2;
	do {
		d2 = InputDriveNumber(eDriveInputAll);
		if (d2 == d1) {
			beep();
		}
	} while (d1 == d2);

	if (d2 == DeviceManager::eNoDrive) {
		AbortInput();
		return;
	}

	wprintw(fInputLineWindow, "%d", d2);

	ShowCursor(false);
	fDeviceManager->ExchangeDrives(d1, d2);
	DisplayDriveStatus(d1);
	DisplayDriveStatus(d2);
	ShowStandardHint();
	UpdateScreen();
}

void CursesFrontend::ProcessUnloadDrive()
{
	ShowDriveInputHint(eDriveInputHintAll);
	ClearInputLine();
	waddstr(fInputLineWindow, "unload drive: ");
	ShowCursor(true);
	UpdateScreen();

	DeviceManager::EDriveNumber d = InputUsedDriveNumber(eDriveInputAll);

	if (d == DeviceManager::eNoDrive) {
		AbortInput();
		return;
	}

	ShowDriveNumber(d);

	bool doUnload = true;

	if (fDeviceManager->DriveIsChanged(d)) {
		ClearInputLine();
		if ( d == DeviceManager::eAllDrives ) {
			waddstr(fInputLineWindow,"there are changed images - really unload them? ");
		} else {
			wprintw(fInputLineWindow, "drive %d is changed - really unload it? ", d);
		}
		ShowYesNoHint();
		ShowCursor(true);
		UpdateScreen();

		EYesNo yn = InputYesNo();

		switch (yn) {
		case eYN_Abort:
			AbortInput();
			return;
		case eYN_Yes:
			ShowYesNo(yn);
			break;
		case eYN_No:
			ShowYesNo(yn);
			doUnload = false;
			break;
		}
	}

	if (doUnload) {
		if (!fDeviceManager->UnloadDiskImage(d)) {
			DPRINTF("unloading disk image %d failed!", d);
		}
	}

	ShowCursor(false);
	DisplayDriveStatus(d);
	ShowStandardHint();
	UpdateScreen();
}

void CursesFrontend::ProcessLoadDrive()
{
	ShowDriveInputHint(eDriveInputHintStandardPlusCassette);
	ClearInputLine();
	waddstr(fInputLineWindow, "load drive: ");
	ShowCursor(true);
	UpdateScreen();

	DeviceManager::EDriveNumber d = InputUnusedOrUnloadableDriveNumber(eDriveInputStandardPlusCassette);

	if (d == DeviceManager::eNoDrive) {
		AbortInput();
		return;
	}

	ShowDriveNumber(d);

	waddstr(fInputLineWindow," file: ");

	unsigned int x,y, width;
	getyx(fInputLineWindow, y, x);
	if ( (x + 10) > fScreenWidth ) {
		x = 0;
		width = fScreenWidth;
	} else {
		width = fScreenWidth - x;
	}

	FileInput input(this, fDirCache);

	if (d == DeviceManager::eCassette) {
		ShowFileInputHint(false);
	} else {
		input.SetEnableVirtualDriveKey(true);
		ShowFileInputHint(true);
	}

	char filename[PATH_MAX];
	filename[0] = 0;
	bool useVirtualDrive = false;

	StringInput::EInputResult res = 
	input.InputString(fInputLineWindow, x, y, width, filename, PATH_MAX, eFileSelectKey, true, &fFilenameHistory);
	fDirCache->ClearDirectoryData();

	if (res == StringInput::eInputAborted) {
		AbortInput();
		return;
	}

	if (res == StringInput::eInputOK) {
		useVirtualDrive = input.InputIsVirtualDrive();
	}
	if ( (res == StringInput::eInputStartSelect) || (filename[0] == 0) ) {
		//DPRINTF("start select: path=\"%s\" filename=\"%s\"", input.GetPath(), input.GetFilename());
		ClearInputLine();

		FileSelect sel(this);
		if (d != DeviceManager::eCassette) {
			sel.SetEnableVirtualDriveKey(true);
			sel.SetEnableDos2xDirectory(true);
		}
		ShowAuxWindow(true);
		bool ok;
		if (filename[0] != 0) {
			ok=sel.Select(fAuxWindow, input.GetPath(), input.GetFilename(), filename, PATH_MAX);
		} else {
			ok=sel.Select(fAuxWindow, NULL, NULL, filename, PATH_MAX);
		}
		ShowAuxWindow(false);
		if (!ok) {
			AbortInput();
			return;
		}
		ClearInputLine();
		useVirtualDrive = sel.InputIsVirtualDrive();
	}

	if (filename[0] == 0) {
		AbortInput();
		return;
	}

	if (d == DeviceManager::eCassette) {
		ProcessTapeEmulation(filename);
		return;
	}

	int len = strlen(filename);
	if (useVirtualDrive) {
		bool ok;
		if (len > 1 && (filename[len-1] == DIR_SEPARATOR)) {
			filename[len-1] = 0;
		}
		struct stat statbuf;
		if (stat(filename, &statbuf)) {
			AERROR("cannot stat directory \"%s\"",filename);
			AbortInput();
			return;
		}
		if ((!S_ISDIR(statbuf.st_mode))) {
			AERROR("not a directory: \"%s\"", filename);
			AbortInput();
			return;
		}

		ClearInputLine();
		waddstr(fInputLineWindow, "virtual drive");

		int densityNum;
		ESectorLength seclen = e128BytesPerSector;
		unsigned int numSectors = 0;

		if (!InputCreateDriveDensity(densityNum, seclen, false, true)) {
			AbortInput();
			return;
		}
		
		bool haveDefault = false;
		if (densityNum == -1) {
			numSectors = Dos2xUtils::EstimateDiskSize(filename, seclen, Dos2xUtils::ePicoName);
			haveDefault = true;
		}

		if (densityNum < 1) {
			if (!InputCreateDriveSectors(numSectors, 720, haveDefault)) {
				AbortInput();
				return;
			}
		}

		switch (densityNum) {
		case 1:
			waddstr(fInputLineWindow,"90k");
			ok = fDeviceManager->CreateVirtualDrive(d, filename, e90kDisk, true);
			break;
		case 2:
			waddstr(fInputLineWindow,"130k");
			ok = fDeviceManager->CreateVirtualDrive(d, filename, e130kDisk, true);
			break;
		case 3:
			waddstr(fInputLineWindow,"180k");
			ok = fDeviceManager->CreateVirtualDrive(d, filename, e180kDisk, true);
			break;
		case 4:
			waddstr(fInputLineWindow,"360k");
			ok = fDeviceManager->CreateVirtualDrive(d, filename, e360kDisk, true);
			break;
		default:
			ok = fDeviceManager->CreateVirtualDrive(d, filename, seclen, numSectors, true);
			break;
		}

		if (!ok) {
			AERROR("creating virtual drive failed");
			AbortInput();
			return;
		}
	} else {
		if (fDeviceManager->LoadDiskImage(d, filename, false, true)) {
			const char* fn = fDeviceManager->GetImageFilename(d);
			if (fn == NULL) {
				DPRINTF("getting filename of newly loaded image failed!");
			} else {
				fFilenameHistory.Add(fn);
			}
		}
	}

	ShowCursor(false);
	DisplayDriveStatus(d);
	ShowStandardHint();
	UpdateScreen();
}

void CursesFrontend::ProcessWriteDrive()
{
	ShowDriveInputHint(eDriveInputHintAllChanged);
	ClearInputLine();
	waddstr(fInputLineWindow, "write drive: ");
	ShowCursor(true);
	UpdateScreen();

	DeviceManager::EDriveNumber d = InputUsedDriveNumber(eDriveInputAll);

	if (d == DeviceManager::eNoDrive) {
		AbortInput();
		return;
	}

	ShowDriveNumber(d, true);

	if (d == DeviceManager::eAllDrives) {
		fDeviceManager->WriteBackImagesIfChanged();
	} else {
		waddstr(fInputLineWindow," file: ");
		unsigned int x,y, width;
		getyx(fInputLineWindow, y, x);
		if ( (x + 10) > fScreenWidth ) {
			x = 0;
			width = fScreenWidth;
		} else {
			width = fScreenWidth - x;
		}

		FileInput input(this, fDirCache);

		char filename[PATH_MAX];
		const char* imgfn = fDeviceManager->GetImageFilename(d);
		if (imgfn) {
			strncpy(filename, imgfn, PATH_MAX-1);
			filename[PATH_MAX-1] = 0;
		} else {
			filename[0] = 0;
		}

		ShowFileInputHint(false);

		StringInput::EInputResult res = 
		input.InputString(fInputLineWindow, x, y, width, filename, PATH_MAX, eFileSelectKey, true, &fFilenameHistory);
		fDirCache->ClearDirectoryData();

		if (res == StringInput::eInputAborted) {
			AbortInput();
			return;
		}
		if ( (res == StringInput::eInputStartSelect) || (filename[0] == 0) ) {
			// DPRINTF("start select: path=\"%s\" filename=\"%s\"", input.GetPath(), input.GetFilename());
			ClearInputLine();
	
			FileSelect sel(this);
			sel.SetEnableDos2xDirectory(true);
			ShowAuxWindow(true);
			bool ok;
			if (filename[0] != 0) {
				ok=sel.Select(fAuxWindow, input.GetPath(), input.GetFilename(), filename, PATH_MAX);
			} else {
				ok=sel.Select(fAuxWindow, NULL, NULL, filename, PATH_MAX);
			}
			ShowAuxWindow(false);
			if (!ok) {
				AbortInput();
				return;
			}
			ClearInputLine();
		}

		if (filename[0] == 0) {
			AbortInput();
			return;
		}
		if (fDeviceManager->WriteDiskImage(d, filename)) {
			const char* fn = fDeviceManager->GetImageFilename(d);
			if (fn == NULL) {
				DPRINTF("getting filename of newly loaded image failed!");
			} else {
				fFilenameHistory.Add(fn);
			}
		}
	}

	ShowCursor(false);
	DisplayDriveStatus(d);
	ShowStandardHint();
	UpdateScreen();
}

void CursesFrontend::ProcessReloadDrive()
{
	ShowDriveInputHint(eDriveInputHintAll);
	ClearInputLine();
	waddstr(fInputLineWindow, "reload drive: ");
	ShowCursor(true);
	UpdateScreen();

	DeviceManager::EDriveNumber d = InputUsedDriveNumber(eDriveInputAll);

	if (d == DeviceManager::eNoDrive) {
		AbortInput();
		return;
	}

	ShowDriveNumber(d, false);

	fDeviceManager->ReloadDrive(d);

	ShowCursor(false);
	DisplayDriveStatus(d);
	ShowStandardHint();
	UpdateScreen();
}

bool CursesFrontend::InputCreateDriveDensity(int& densityNum, ESectorLength& seclen, bool enableQD, bool enableAutoSectors)
{
	int ch;

	ShowCreateDriveHint(enableQD);
	waddstr(fInputLineWindow," size: ");
	UpdateScreen();

	do {
		ch = GetCh(true);
	} while ( (!IsAbortChar(ch)) &&
		  (ch != '1') && (ch != '2') && (ch != '3') && (ch != '4') &&
		  (ch != 's') && (ch != 'S') && (ch != 'd') && (ch != 'D') &&
		  !( enableQD && ((ch == 'f') || (ch == 'F') ||
                     (ch == '5') || (ch == '6') || (ch == '7') || (ch == '8') )
                   )
		  );

	if (IsAbortChar(ch)) {
		return false;
	}
	switch (ch) {
	case '1':
	case '2':
	case '3':
	case '4':
		densityNum = ch - '0';
		return true;
	case 's':
	case 'd':
	case 'S':
	case 'D':
	case 'f':
	case 'F':
	case '5':
	case '6':
	case '7':
	case '8':
		densityNum = 0;
		if (enableAutoSectors && (ch == 's' || ch == 'd')) {
			densityNum = -1;
			waddstr(fInputLineWindow,"auto ");
		} else {
			waddstr(fInputLineWindow,"custom ");
		}
		switch (ch) {
		case 's':
		case 'S':
			waddstr(fInputLineWindow,"SD");
			seclen = e128BytesPerSector;
			break;
		case 'd':
		case 'D':
			waddstr(fInputLineWindow,"DD");
			seclen = e256BytesPerSector;
			break;
		case 'f':
		case 'F':
			waddstr(fInputLineWindow,"QD");
			seclen = e512BytesPerSector;
			break;
		case '5':
			waddstr(fInputLineWindow,"1k");
			seclen = e1kPerSector;
			break;
		case '6':
			waddstr(fInputLineWindow,"2k");
			seclen = e2kPerSector;
			break;
		case '7':
			waddstr(fInputLineWindow,"4k");
			seclen = e4kPerSector;
			break;
		case '8':
			waddstr(fInputLineWindow,"8k");
			seclen = e8kPerSector;
			break;
		default:
			DPRINTF("unhandeled density %d in InputCreateDriveDensity", ch);
			return false;
		}
		return true;
	default:
		DPRINTF("unhandeled option %d in InputCreateDriveDensity", ch);
		break;
	}
	return false;
}

bool CursesFrontend::InputCreateDriveSectors(unsigned int& numSectors, int minimumSectors, bool haveDefault)
{
	int myNumSectors;
	waddstr(fInputLineWindow," sectors: ");

	unsigned int x,y, width;
	getyx(fInputLineWindow, y, x);
	width = 6;
	if ( (x + 6) > fScreenWidth ) {
		x = 0;
		if (fScreenWidth < 6) {
			width = fScreenWidth;
		}
	}

	StringInput input(this);

	char sectors[6];
	if (haveDefault) {
		snprintf(sectors, 6, "%u", numSectors);
	} else {
		sectors[0] = 0;
	}

	ShowImageSizeHint(minimumSectors);

	do {
		StringInput::EInputResult res = 
			input.InputString(fInputLineWindow, x, y, width, sectors, 6, -1, true, &fImageSizeHistory);
		if (res == StringInput::eInputAborted) {
			return false;
		}
		myNumSectors = atoi(sectors);
		if ( (myNumSectors >= minimumSectors) && (myNumSectors <= 65535) ) {
			numSectors = myNumSectors;
			return true;
		} else {
			beep();
		}
	} while (1);
}

void CursesFrontend::ProcessCreateDrive()
{
	ShowDriveInputHint(eDriveInputHintStandard);
	ClearInputLine();
	waddstr(fInputLineWindow, "create drive: ");
	ShowCursor(true);
	UpdateScreen();

	DeviceManager::EDriveNumber d = InputUnusedOrUnloadableDriveNumber(eDriveInputStandard);

	if (d == DeviceManager::eNoDrive) {
		AbortInput();
		return;
	}

	ShowDriveNumber(d);

	int densityNum;
	unsigned int numSectors;
	ESectorLength seclen;

	if (!InputCreateDriveDensity(densityNum, seclen, true, false)) {
		AbortInput();
		return;
	}
	if (densityNum < 1) {
		if (!InputCreateDriveSectors(numSectors, 1, false)) {
			AbortInput();
			return;
		}
	}
	switch (densityNum) {
	case 1:
		waddstr(fInputLineWindow,"90k");
		fDeviceManager->CreateAtrMemoryImage(d, e90kDisk, true);
		break;
	case 2:
		waddstr(fInputLineWindow,"130k");
		fDeviceManager->CreateAtrMemoryImage(d, e130kDisk, true);
		break;
	case 3:
		waddstr(fInputLineWindow,"180k");
		fDeviceManager->CreateAtrMemoryImage(d, e180kDisk, true);
		break;
	case 4:
		waddstr(fInputLineWindow,"360k");
		fDeviceManager->CreateAtrMemoryImage(d, e360kDisk, true);
		break;
	default:
		fDeviceManager->CreateAtrMemoryImage(d, seclen, numSectors, true);
		break;
	}

	ShowCursor(false);
	DisplayDriveStatus(d);
	ShowStandardHint();
	UpdateScreen();
}

void CursesFrontend::ProcessSetTraceLevel()
{
	ShowTraceLevelHint();
	ClearInputLine();
	waddstr(fInputLineWindow, "set trace level: ");
	ShowCursor(true);
	UpdateScreen();

	int ch;
	do {
		ch = GetCh(true);
		if (IsAbortChar(ch)) {
			AbortInput();
			return;
		}
		if ( (ch >= '0') && (ch <= '3') ) {
			break;
		}
		beep();
	} while(1);

	int level = ch - '0';

	wprintw(fInputLineWindow, "%d", level);

	ShowCursor(false);
	SetTraceLevel(level);
	DisplayStatusLine();
	ShowStandardHint();
	UpdateScreen();
}

void CursesFrontend::ProcessInstallPrinterHandler()
{
	if (fDeviceManager->DriveInUse(DeviceManager::ePrinter)) {
		ALOG("printer handler already installed");
		return;
	}
	ShowPrinterEOLHint();
	ClearInputLine();
	waddstr(fInputLineWindow, "install printer: EOL=");
	ShowCursor(true);
	UpdateScreen();

	PrinterHandler::EEOLConversion conv;
	int ch;
	do {
		ch = GetCh(true);
		if (IsAbortChar(ch)) {
			AbortInput();
			return;
		}
		if ((ch == 'r') || (ch == 'R')) {
			conv = PrinterHandler::eRaw;
			waddstr(fInputLineWindow, "raw");
			break;
		}
		if ((ch == 'l') || (ch == 'L')) {
			conv = PrinterHandler::eLF;
			waddstr(fInputLineWindow, "LF");
			break;
		}
		if ((ch == 'c') || (ch == 'C')) {
			conv = PrinterHandler::eLF;
			waddstr(fInputLineWindow, "CR");
			break;
		}
		if ((ch == 'b') || (ch == 'B')) {
			conv = PrinterHandler::eCRLF;
			waddstr(fInputLineWindow, "CR+LF");
			break;
		}
		beep();
	} while(1);

	waddstr(fInputLineWindow," file: ");
	unsigned int x,y, width;
	getyx(fInputLineWindow, y, x);
	if ( (x + 10) > fScreenWidth ) {
		x = 0;
		width = fScreenWidth;
	} else {
		width = fScreenWidth - x;
	}

	FileInput input(this, fDirCache);
	input.SetEnableVirtualDriveKey(false);

	char filename[PATH_MAX];
	filename[0] = 0;

	ShowPrinterFileHint();

	StringInput::EInputResult res = 
	input.InputString(fInputLineWindow, x, y, width, filename, PATH_MAX, eFileSelectKey, true, &fPrinterHistory);
	fDirCache->ClearDirectoryData();

	if (res == StringInput::eInputAborted) {
		AbortInput();
		return;
	}

	if ( (res == StringInput::eInputStartSelect) || (filename[0] == 0) ) {
		//DPRINTF("start select: path=\"%s\" filename=\"%s\"", input.GetPath(), input.GetFilename());
		ClearInputLine();

		FileSelect sel(this);
		sel.SetEnableVirtualDriveKey(false);
		sel.SetEnableDos2xDirectory(false);
		ShowAuxWindow(true);
		bool ok;
		if (filename[0] != 0) {
			ok=sel.Select(fAuxWindow, input.GetPath(), input.GetFilename(), filename, PATH_MAX);
		} else {
			ok=sel.Select(fAuxWindow, NULL, NULL, filename, PATH_MAX);
		}
		ShowAuxWindow(false);
		if (!ok) {
			AbortInput();
			return;
		}
		ClearInputLine();
	}

	if (filename[0] == 0) {
		AbortInput();
		return;
	}

	if (fDeviceManager->InstallPrinterHandler(filename, conv)) {
		const char* fn = fDeviceManager->GetPrinterFilename();
		if (fn == NULL) {
			DPRINTF("getting filename of newly loaded image failed!");
		} else {
			fPrinterHistory.Add(fn);
		}
	}

	ShowCursor(false);
	DisplayPrinterStatus();
	ShowStandardHint();
	UpdateScreen();
}

void CursesFrontend::ProcessUninstallPrinterHandler()
{
	ClearInputLine();
	waddstr(fInputLineWindow, "uninstall printer");

	if (fDeviceManager->DriveInUse(DeviceManager::ePrinter)) {
		if (!fDeviceManager->RemovePrinterHandler()) {
			ALOG("uninstalling printer handler failed");
		}
	} else {
		ALOG("no printer handler installed");
	}
	DisplayPrinterStatus();
	UpdateScreen();
}

void CursesFrontend::ProcessFlushPrinterData()
{
	ClearInputLine();
	waddstr(fInputLineWindow, "flush printer data");
	if (fDeviceManager->DriveInUse(DeviceManager::ePrinter)) {
		fDeviceManager->FlushPrinterData();
	}
	DisplayPrinterStatus();
	UpdateScreen();
}

void CursesFrontend::ProcessSetCommandLine()
{
	werase(fBottomLineWindow);
	wmove(fBottomLineWindow, 0, 0);
	waddstr(fBottomLineWindow,"'r'=RING, 'd'=DSR, 'c'=CTS, 'n'=none, 'q'=abort");
	ClearInputLine();
	waddstr(fInputLineWindow, "set command line: ");
	ShowCursor(true);
	UpdateScreen();

	int ch;
	do {
		ch = GetCh(true);
		if (IsAbortChar(ch)) {
			AbortInput();
			return;
		}
		if ( (ch == 'R') || (ch == 'r') ||
		     (ch == 'D') || (ch == 'd') ||
		     (ch == 'C') || (ch == 'c') ||
		     (ch == 'N') || (ch == 'n') ) {
			break;
		}
		beep();
	} while(1);

	switch (ch) {
	case 'R':
	case 'r':
		wprintw(fInputLineWindow, "RING");
		fDeviceManager->SetSioServerMode(SIOWrapper::eCommandLine_RI);
		break;
	case 'D':
	case 'd':
		wprintw(fInputLineWindow, "DSR");
		fDeviceManager->SetSioServerMode(SIOWrapper::eCommandLine_DSR);
		break;
	case 'C':
	case 'c':
		wprintw(fInputLineWindow, "CTS");
		fDeviceManager->SetSioServerMode(SIOWrapper::eCommandLine_CTS);
		break;
	case 'N':
	case 'n':
		wprintw(fInputLineWindow, "none");
		fDeviceManager->SetSioServerMode(SIOWrapper::eCommandLine_None);
		break;
	}

	ShowCursor(false);
	DisplayStatusLine();
	ShowStandardHint();
	UpdateScreen();
}

void CursesFrontend::ProcessSetHighSpeed()
{
	ShowYesNoHint();
	ClearInputLine();
	waddstr(fInputLineWindow, "enable highspeed SIO: ");
	ShowCursor(true);
	UpdateScreen();

	EYesNo yn = InputYesNo();

	switch (yn) {
	case eYN_Abort:
		AbortInput();
		return;
	case eYN_Yes:
		ShowYesNo(yn);
		fDeviceManager->SetHighSpeedMode(true);
		break;
	case eYN_No:
		ShowYesNo(yn);
		fDeviceManager->SetHighSpeedMode(false);
		break;
	}

	ShowCursor(false);
	DisplayStatusLine();
	ShowStandardHint();
	UpdateScreen();
}

void CursesFrontend::ProcessSetSioTiming()
{
	ShowSioTimingHint();
	ClearInputLine();
	waddstr(fInputLineWindow, "set SIO timing: ");
	ShowCursor(true);
	UpdateScreen();

	int ch;
	do {
		ch = GetCh(true);
		if (IsAbortChar(ch)) {
			AbortInput();
			return;
		}
		if ( (ch == 'R') || (ch == 'r') ||
		     (ch == 'S') || (ch == 's') ) {
			break;
		}
		beep();
	} while(1);

	switch (ch) {
	case 'R':
	case 'r':
		waddstr(fInputLineWindow, "relaxed");
		fDeviceManager->SetSioTiming(SIOWrapper::eRelaxedTiming);
		break;
	case 'S':
	case 's':
		waddstr(fInputLineWindow, "strict");
		fDeviceManager->SetSioTiming(SIOWrapper::eStrictTiming);
		break;
	}

	ShowCursor(false);
	DisplayStatusLine();
	ShowStandardHint();
	UpdateScreen();
}

void CursesFrontend::ProcessSetHighSpeedParameters()
{
	uint8_t divisor = fDeviceManager->GetHighSpeedPokeyDivisor();
	unsigned int baud = fDeviceManager->GetHighSpeedBaudrate();
	unsigned int defaultBaudrate = fDeviceManager->GetSIOWrapper()->GetBaudrateForPokeyDivisor(divisor);

	char param[20];

	if (baud == defaultBaudrate) {
		snprintf(param, 20, "%d", divisor);
	} else {
		snprintf(param, 20, "%d,%d", divisor, baud);
	}

	ShowHighSpeedParametersHint();
	ClearInputLine();
	waddstr(fInputLineWindow, "high speed pokey divisor: ");
	ShowCursor(true);
	UpdateScreen();

	StringInput input(this);

	unsigned int x, y, width;

	getyx(fInputLineWindow, y, x);
	width=10;
	if ( (x+width) > fScreenWidth ) {
		x = 0;
		if (fScreenWidth < width) {
			width = fScreenWidth;
		}
	}

	do {
		StringInput::EInputResult res = 
			input.InputString(fInputLineWindow, x, y, width, param, 20, -1, true, &fHighSpeedHistory);
		if (res == StringInput::eInputAborted) {
			AbortInput();
			return;
		}
		if (MiscUtils::ParseHighSpeedParameters(param, divisor, baud)) {
			break;
		} else {
			beep();
		}
	} while (1);

	if (fDeviceManager->SetHighSpeedParameters(divisor, baud)) {
		ALOG("Configured high speed mode to pokey divisor %d / %d baud",
			divisor, fDeviceManager->GetHighSpeedBaudrate());
	} else {
		AERROR("setting high speed parameters (divisor %d, baud %d) failed!", divisor, baud);
	}

	ShowCursor(false);
	DisplayStatusLine();
	ShowStandardHint();
	UpdateScreen();
}


void CursesFrontend::ProcessSetXF551Mode()
{
	ShowYesNoHint();
	ClearInputLine();
	waddstr(fInputLineWindow, "enable XF551 commands: ");
	ShowCursor(true);
	UpdateScreen();

	EYesNo yn = InputYesNo();

	switch (yn) {
	case eYN_Abort:
		AbortInput();
		return;
	case eYN_Yes:
		ShowYesNo(yn);
		fDeviceManager->EnableXF551Mode(true);
		break;
	case eYN_No:
		ShowYesNo(yn);
		fDeviceManager->EnableXF551Mode(false);
		break;
	}

	ShowCursor(false);
	DisplayStatusLine();
	ShowStandardHint();
	UpdateScreen();
}

void CursesFrontend::ProcessShowDirectory()
{
	ShowDriveInputHint(eDriveInputHintStandard);
	ClearInputLine();
	waddstr(fInputLineWindow, "directory of drive: ");
	ShowCursor(true);
	UpdateScreen();

	DeviceManager::EDriveNumber d = InputUsedDriveNumber(eDriveInputStandard);

	ShowCursor(false);

	if (d == DeviceManager::eNoDrive) {
		AbortInput();
		return;
	}

	ShowDriveNumber(d);

	ShowDos2Directory(fDeviceManager->GetDiskImage(d));

	ShowStandardHint();
	InitTopLine();

	UpdateScreen();
}

void CursesFrontend::ProcessFormatDrive()
{
	ShowDriveInputHint(eDriveInputHintStandard);
	ClearInputLine();
	waddstr(fInputLineWindow, "format drive: ");
	ShowCursor(true);
	UpdateScreen();

	DeviceManager::EDriveNumber d = InputUsedDriveNumber(eDriveInputStandard);

	if (d == DeviceManager::eNoDrive) {
		AbortInput();
		return;
	}

	ShowDriveNumber(d);

	if (fDeviceManager->DriveIsVirtualImage(d)) {
		AERROR("formatting virtual images is not allowed");
		AbortInput();
		return;
	}

	RCPtr<AtrImage> img = fDeviceManager->GetAtrImage(d);

	if (img.IsNull()) {
		AERROR("Formatting ATP images is currently unsupported");
		AbortInput();
		return;
	}

	unsigned int seclen = img->GetSectorLength();
	if (seclen != 128 && seclen != 256) {
		AERROR("Only SD and DD images are supported");
		AbortInput();
		return;
	}

	werase(fBottomLineWindow);
	wmove(fBottomLineWindow, 0, 0);
	waddstr(fBottomLineWindow, "'d' = DOS 2.x, 'm' = MyDOS 'q' = abort");
	waddstr(fInputLineWindow, " format type: ");
	UpdateScreen();

	int ch;
	do {
		ch = GetCh(true);
		if (IsAbortChar(ch)) {
			AbortInput();
			return;
		}
		if ( (ch == 'd') || (ch == 'D') || (ch == 'm') || (ch == 'M') ) {
			break;
		}
		beep();
	} while(1);

	RCPtr<Dos2xUtils> utils = new Dos2xUtils(img);

	bool ok = true;
	switch (ch) {
	case 'd':
	case 'D':
		waddstr(fInputLineWindow,"DOS 2.x");
		ok = utils->SetDosFormat(Dos2xUtils::eDos2x);
		break;
	case 'm':
	case 'M':
		waddstr(fInputLineWindow,"MyDOS");
		ok = utils->SetDosFormat(Dos2xUtils::eMyDos);
		break;
	}
	if (ok) {
		ok = utils->InitVTOC();
	}
	if (!ok) {
		AERROR("formatting drive failed");
	}

	ShowCursor(false);
	DisplayDriveStatus(d);
	ShowStandardHint();
	InitTopLine();
	UpdateScreen();
}

static const char* const helpText[] =
	{
		"l     load image into drive",
		"w     write drive(s) to image(s)",
		"c     create drive",
		"u     unload drive(s)",
		"x     exchange (swap) drives",
		"d     display DOS 2.x directory of drive",
		"f     format drive (clear image, write VTOC and directory)",
		"r     reload virtual drive",
		"p     write protect drive(s)",
		"P     write un-protect drive(s)",
		"a     activate drive(s)",
		"A     de-activate drive(s)",
		"i     install printer handler",
		"I     uninstall printer handler",
		"F     flush queued printer data",
	        "t     set trace level",
		"C     set command line",
		"s     enable/disable high speed SIO",
		"S     set high speed pokey divisor/baudrate",
		"T     set strict/relaxed SIO timing",
		"X     enable/disable XF551 commands",
		"^L    redraw screen",
		"h     show help screen",
		"q     quit atariserver",
		0
	};

void CursesFrontend::ProcessShowHelp()
{
	ClearInputLine();
	ShowText("[ help screen ]", helpText);
	ShowStandardHint();
	InitTopLine();
	UpdateScreen();
}

void CursesFrontend::AddFilenameHistory(const char* string)
{
	fFilenameHistory.Add(string);
}

bool CursesFrontend::ShowDos2Directory(const RCPtr<DiskImage>& image, bool beQuiet)
{
	if (image.IsNull()) {
		return false;
	}

	unsigned int seclen = image->GetSectorLength();
	if (seclen != 128 && seclen != 256) {
		AERROR("Only SD and DD images are supported");
		return false;
	}

	RCPtr<Dos2xUtils> utils = new Dos2xUtils(image);

	RCPtr<Dos2xUtils::Dos2Dir> dir = utils->GetDos2Directory(beQuiet);
	if (dir.IsNull()) {
		return false;
	}

	bool oldAux = ShowAuxWindow(true);
	bool oldCursor = ShowCursor(false);

	unsigned int columns, lines;
	getmaxyx(fAuxWindow, lines, columns);

	unsigned int cols = columns / Dos2xUtils::Dos2Dir::eFileWidth;
	if (cols < 1) {
		cols = 1;
	}

	SetTopLineFilename(image->GetFilename(), false);

	ShowPagerHint();

	unsigned int x,y;
	unsigned int filenum = 0;

	bool done = false;

	while (!done) {
		unsigned int startFilenum = filenum;
		x = 0;
		y = 0;
		werase(fAuxWindow);

		while ( (filenum <= dir->GetNumberOfFiles() + 1) && (y < lines) ) {
			if (filenum == dir->GetNumberOfFiles()) {
				if (x != 0) {
					DPRINTF("DisplayDos2Directory: x != 0!");
				}
				wmove(fAuxWindow, y, 0);
				wprintw(fAuxWindow, "%d free sectors", dir->GetFreeSectors());
			} else {
				wmove(fAuxWindow, y, x*Dos2xUtils::Dos2Dir::eFileWidth);
				waddstr(fAuxWindow, dir->GetFile(filenum));
			}

			filenum++;
			if (filenum == dir->GetNumberOfFiles()) {
				x=0;
				y++;
			} else if (filenum == dir->GetNumberOfFiles() + 1) {
				done = true;
			} else {
				x++;
				if (x==cols) {
					x = 0;
					y ++;
				}
			}
		}
		UpdateScreen();
		int ch = GetCh(false);
		if (ch == KEY_RESIZE) {
			HandleResize(true);
			ShowPagerHint();
			SetTopLineFilename(image->GetFilename(), false);
			ClearInputLine();
			done = false;
			filenum = startFilenum;

			getmaxyx(fAuxWindow, lines, columns);
			cols = columns / Dos2xUtils::Dos2Dir::eFileWidth;
			if (cols < 1) {
				cols = 1;
			}
		} else {
			if (IsAbortChar(ch)) {
				done = true;
			}
		}
	}

	werase(fAuxWindow);
	ShowStandardHint();
	ShowAuxWindow(oldAux);
	ShowCursor(oldCursor);
	return true;
}

void CursesFrontend::ShowText(const char* title, const char* const * text)
{
	if (text == NULL) {
		return;
	}

	bool oldAux = ShowAuxWindow(true);
	bool oldCursor = ShowCursor(false);

	unsigned int columns, lines;
	getmaxyx(fAuxWindow, lines, columns);

	unsigned int currentLine;
	unsigned int currentTextLine = 0;
	const char* currentTextPtr = text[currentTextLine];
	unsigned int len;

	bool done = false;

	if (title) {
		SetTopLineString(title);
	}
	ShowPagerHint();

	while (!done) {
		currentLine = 0;
		unsigned int startTextLine = currentTextLine;
		const char* startTextPtr = currentTextPtr;
		werase(fAuxWindow);

		while ( currentTextPtr && (currentLine < lines) ) {
			len = strlen(currentTextPtr);
			if (len > columns) {
				len = columns;
				while ( (len>0) && (currentTextPtr[len-1] != ' ') && (currentTextPtr[len-1] != '-') ) {
					len--;
				}
				if (len == 0) {
					len = columns;
				}
			}
			wmove(fAuxWindow, currentLine, 0);
			if (len > 0) {
				waddnstr(fAuxWindow, currentTextPtr, len);
				currentTextPtr += len;
			}
			currentLine++;
			if ( *currentTextPtr == 0) {
				currentTextPtr = text[++currentTextLine];
				if (currentTextPtr == NULL) {
					done = true;
				}
			}
		}

		UpdateScreen();
		int ch = GetCh(false);
		if (ch == KEY_RESIZE) {
			HandleResize(true);
			ShowPagerHint();
			if (title) {
				SetTopLineString(title);
			}
			ClearInputLine();
			currentTextLine = startTextLine;
			currentTextPtr = startTextPtr;
			done = false;

			getmaxyx(fAuxWindow, lines, columns);
		} else {
			if (IsAbortChar(ch)) {
				done = true;
			}
			while (currentTextPtr && (*currentTextPtr == 0) ) {
				currentTextPtr = text[++currentTextLine];
			}
			if (currentTextPtr == NULL) {
				done = true;
			}
		}
	}

	werase(fAuxWindow);
	ShowAuxWindow(oldAux);
	ShowCursor(oldCursor);
}

void CursesFrontend::ProcessTapeEmulation(const char* filename)
{
	if ( (filename[0] == 0) || (!fDeviceManager->LoadCasImage(filename)) ) {
		AbortInput();
		return;
	}

	RCPtr<CasHandler> cas = fDeviceManager->GetCasHandler();
	fCasHistory.Add(cas->GetFilename());

	ShowCursor(false);
	ShowCasWindow(true);
	ShowCasHint();
	DisplayCasStatus();
	UpdateScreen();

	bool done = false;

	while (!done) {
		CasHandler::EPlayResult ret;

		ret = cas->DoPlaying();
		int ch;

		if (ret == CasHandler::eGotSignal) {
			if (GotSigWinCh()) {
				ch = KEY_RESIZE;
			} else {
				continue;
			}
		} else {
			if (ret == CasHandler::eWaitForStart) {
				ch = GetCh(false);
			} else {
				ch = wgetch(fInputLineWindow);
			}
			if (ch == KEY_RESIZE) {
				//DPRINTF("got KEY_RESIZE from ncurses!");
				continue;
			}
		}

		if (ch == ' ') {
			switch (cas->GetState()) {
			case CasHandler::eStatePaused:
			case CasHandler::eStateDone:
				cas->SetPause(false);
				break;
			case CasHandler::eStateGap:
			case CasHandler::eStatePlaying:
				cas->SetPause(true);
				break;
			}
		} else if (ch == 'g') {
			cas->SkipGap();
		} else {
			if (IsAbortChar(ch)) {
				cas->AbortTapePlayback();
				done = true;
				continue;
			}

			switch (ch) {
			case KEY_LEFT:
				cas->PauseIfPlaying();
				if (!cas->SeekPrevBlock()) {
					beep();
				}
				break;
			case KEY_RIGHT:
				cas->PauseIfPlaying();
				if (!cas->SeekNextBlock()) {
					beep();
				}
				break;
			case KEY_UP:
				cas->PauseIfPlaying();
				if (!cas->SeekPrevBlock(20)) {
					beep();
				}
				break;
			case KEY_DOWN:
				cas->PauseIfPlaying();
				if (!cas->SeekNextBlock(20)) {
					beep();
				}
				break;
			case KEY_PPAGE:
				cas->PauseIfPlaying();
				if (!cas->SeekPrevPart()) {
					beep();
				}
				break;
			case KEY_NPAGE:
				cas->PauseIfPlaying();
				if (!cas->SeekNextPart()) {
					beep();
				}
				break;
			case KEY_HOME:
				cas->PauseIfPlaying();
				if (!cas->SeekStart()) {
					beep();
				}
				break;
			case KEY_END:
				cas->PauseIfPlaying();
				if (!cas->SeekEnd()) {
					beep();
				}
				break;
			case 12:
			case KEY_RESIZE:
				HandleResize();
				DisplayCasStatus();
				ShowCasHint();
				break;
			default:
				beep();
			}
		}
	}

	ShowCasWindow(false);
	ShowStandardHint();
	UpdateScreen();
	fDeviceManager->UnloadCasImage();
}
