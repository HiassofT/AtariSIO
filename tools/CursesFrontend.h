#ifndef CURSESFRONTEND_H
#define CURSESFRONTEND_H

/*
   CursesFrontend.h - curses frontend for atariserver

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

#include <curses.h>
#include <panel.h>
#include "DeviceManager.h"
#include "History.h"
#include "DirectoryCache.h"

class CursesFrontend {
public:
	CursesFrontend(RCPtr<DeviceManager>& manager, bool useColor = true);

	virtual ~CursesFrontend();

	void HandleResize(bool redrawDriveStatusOnly = false);

	void InitWindows(bool resetCursor = true);

	void InitDriveStatusWindows();
	void InitTopAndBottomLines();

	// drive = eDrive[1-8]: only specified drive
	// drive = eAllDrives  : all drives
	void DisplayDriveStatus(DeviceManager::EDriveNumber drive = DeviceManager::eAllDrives);

	void DisplayDriveNumber(DeviceManager::EDriveNumber drive);
	void DisplayDriveChangedStatus(DeviceManager::EDriveNumber drive);
	void DisplayDriveProtectedStatus(DeviceManager::EDriveNumber drive);
	void DisplayDriveDensity(DeviceManager::EDriveNumber drive);
	void DisplayDriveSectors(DeviceManager::EDriveNumber drive);
	void DisplayDriveFilename(DeviceManager::EDriveNumber drive);

	void DisplayPrinterStatus();
	void DisplayPrinterRunningStatus();
	void DisplayPrinterNumber();
	void DisplayPrinterEOLConversion();
	void DisplayPrinterFilename();

	void DisplayCasStatus();

	void DisplayCasText();

	void DisplayCasFilename();
	void DisplayCasDescription();
	void DisplayCasBlock();
	void DisplayCasState();

	// return old status
	bool ShowCursor(bool on);

	bool ShowDos2Directory(const RCPtr<DiskImage>& image, bool beQuiet = false);

	void ShowText(const char* title, const char* const* text);

	void AddLogLine(const char* str);
	void PrintLogLine(const char* format, ...)
		__attribute__ ((format (printf, 2, 3))) ;

	// CursesTracer methods
	void StartLogLine();
	void EndLogLine();
	void AddLogString(const char* str);
	void AddLogHighlightString(const char* str);
	void AddLogOKString(const char* str);
	void AddLogDebugString(const char* str);
	void AddLogWarningString(const char* str);
	void AddLogErrorString(const char* str);

	// update screen buffers
	void UpdateScreen();

	int GetCh(bool ignoreResize);

	void SetTraceLevel(int level);

	int GetTraceLevel() const { return fTraceLevel; }

	void DisplayStatusLine();

	bool ProcessQuit();
	void ProcessLoadDrive();
	void ProcessWriteDrive();
	void ProcessReloadDrive();
	void ProcessCreateDrive();
	void ProcessWriteProtectDrive();
	void ProcessUnprotectDrive();
	void ProcessActivateDrive();
	void ProcessDeactivateDrive();
	void ProcessExchangeDrives();
	void ProcessUnloadDrive();
	void ProcessSetTraceLevel();
	void ProcessSetCommandLine();
	void ProcessSetHighSpeed();
	void ProcessSetSioTiming();
	void ProcessSetXF551Mode();
	void ProcessShowDirectory();
	void ProcessShowHelp();
	void ProcessFormatDrive();

	void ProcessInstallPrinterHandler();
	void ProcessUninstallPrinterHandler();
	void ProcessFlushPrinterData();

	void ProcessTapeEmulation(const char* filename);

	void ProcessSetHighSpeedParameters();

	void AddFilenameHistory(const char* string);

	// return old status
	bool ShowAuxWindow(bool on);
	bool ShowCasWindow(bool on);

	void IndicateGotSigWinCh() { fGotSigWinCh = true; }
	bool GotSigWinCh() { return fGotSigWinCh; }

	void InitTopLine();
	void SetTopLineString(const char* string);
	void SetTopLineFilename(const char* string, bool appenSlash);

	unsigned int GetScreenWidth() { return fScreenWidth; }
	unsigned int GetScreenHeight() { return fScreenHeight; }

	enum { eVirtualDriveKey = 22 }; // control-V

	inline void SetAskBeforeQuit(bool flag)
	{
		fAskBeforeQuit = flag;
	}

	inline bool GetAskBeforeQuit()
	{
		return fAskBeforeQuit;
	}

private:
	friend class FileSelect;
	void ShowHint(const char* string);
	RCPtr<DirectoryCache>& GetDirectoryCache() { return fDirCache; }

	attr_t GetAuxColorStandard() { return fAuxColorStandard; }
	attr_t GetAuxColorSelected() { return fAuxColorSelected; }
	WINDOW* GetInputLineWindow() { return fInputLineWindow; }

private:
	void InternalProcessWriteProtectDrive(bool protect);
	void InternalProcessActivateDrive(bool act);
	bool InputCreateDriveDensity(int& densityNum, ESectorLength& seclen, bool enableQD, bool enableAutoSectors);
	bool InputCreateDriveSectors(unsigned int& numSectors, int minimumSectors, bool haveDefault);

	bool CalculateWindowPositions(
		int& toplineStart, int& toplineHeight,
		int& drivestatusStart, int& drivestatusHeight,
		int& logtitleStart, int& logtitleHeight,
		int& logStart, int& logHeight,
		int& bottomStart, int& bottomHeight,
		int& inputStart, int& inputHeight,
		int& auxStart, int& auxHeight);

	void ClearInputLine();

	bool IsAbortChar(int ch);
	void AbortInput();

	enum EYesNo {
		eYN_Abort = -1,
		eYN_No = 0,
		eYN_Yes = 1
	};
	EYesNo InputYesNo();

	void ShowStandardHint();
	void ShowYesNoHint();
	void ShowPagerHint();
	void ShowFileInputHint(bool enableVirtualDrive);
	void ShowTraceLevelHint();
	void ShowCreateDriveHint(bool enableQD);
	void ShowImageSizeHint(int minimumSectors);
	void ShowSioTimingHint();
	void ShowHighSpeedParametersHint();
	void ShowCasHint();

	void ShowPrinterEOLHint();
	void ShowPrinterFileHint();

	enum EDriveInputType {
		eDriveInputStandard,
		eDriveInputAll,
		eDriveInputAllPrinterRC,
		eDriveInputStandardPlusCassette
	};

	DeviceManager::EDriveNumber InputDriveNumber(EDriveInputType type = eDriveInputStandard);
	DeviceManager::EDriveNumber InputUsedDriveNumber(EDriveInputType type = eDriveInputStandard);
	DeviceManager::EDriveNumber InputVirtualDriveNumber(EDriveInputType type = eDriveInputStandard);
	DeviceManager::EDriveNumber InputUnusedDriveNumber(EDriveInputType type = eDriveInputStandard);
	DeviceManager::EDriveNumber InputUnusedOrUnloadableDriveNumber(EDriveInputType type = eDriveInputStandard);

	enum EDriveInputHintType {
		eDriveInputHintStandard,
		eDriveInputHintAll,
		eDriveInputHintAllChanged,
		eDriveInputHintAllPrinterRC,
		eDriveInputHintStandardPlusCassette
	};
	void ShowDriveInputHint(EDriveInputHintType type);

	void ShowDriveNumber(DeviceManager::EDriveNumber d, bool allChanged=false);
	void ShowYesNo(EYesNo yn);

	enum { eFileSelectKey = 6 }; // ^F

	unsigned int fScreenWidth, fScreenHeight;

	// permanent windows
	WINDOW* fTopLineWindow;
	WINDOW* fDriveStatusWindow;
	WINDOW* fCasWindow;
	WINDOW* fStatusLineWindow;
	WINDOW* fLogWindow;
	WINDOW* fBottomLineWindow;
	WINDOW* fInputLineWindow;
	WINDOW* fAuxWindow;

	PANEL* fTopLinePanel;
	PANEL* fDriveStatusPanel;
	PANEL* fCasPanel;
	PANEL* fStatusLinePanel;
	PANEL* fLogPanel;
	PANEL* fBottomLinePanel;
	PANEL* fInputLinePanel;
	PANEL* fAuxPanel;

	bool fFirstLogLine;

	RCPtr<DeviceManager> fDeviceManager;

	// drive display positions
	unsigned int fDriveYStart;
	unsigned int fDriveNoX;
	unsigned int fDriveChangedX;
	unsigned int fDriveProtectedX;
	unsigned int fDriveDensityX;
	unsigned int fDriveSectorsX;
	unsigned int fDriveFilenameX;
	unsigned int fDriveFilenameLength;

	// cas display positions
	unsigned int fCasFilenameY;
	unsigned int fCasDescriptionY;
	unsigned int fCasBlockY;
	unsigned int fCasPartY;
	unsigned int fCasBaudY;
	unsigned int fCasGapY;
	unsigned int fCasLengthY;
	unsigned int fCasStateY;
	unsigned int fCasXStart;

	// log window colors
	attr_t fLogColorStandard;
	attr_t fLogColorHighlight;
	attr_t fLogColorOK;
	attr_t fLogColorDebug;
	attr_t fLogColorWarning;
	attr_t fLogColorError;

	// drive status window colors
	attr_t fDriveColorStandard;
	attr_t fDriveColorInactive;
	attr_t fDriveColorWritable;
	attr_t fDriveColorWriteProtected;
	attr_t fDriveColorChanged;

	// cas window colors
	attr_t fCasColorStandard;
	attr_t fCasColorInfo;
	
	attr_t fStatusColorStandard;

	// printer status colors
	attr_t fPrinterColorSpawned;
	attr_t fPrinterColorError;

	attr_t fAuxColorStandard;
	attr_t fAuxColorSelected;

	int fTraceLevel;

	History fFilenameHistory;
	History fCasHistory;
	History fPrinterHistory;
	History fImageSizeHistory;
	History fHighSpeedHistory;
	RCPtr<DirectoryCache> fDirCache;

	bool fCursorStatus;
	bool fAuxWindowStatus;
	bool fCasWindowStatus;

	bool fGotSigWinCh;

	bool fAlreadyReportedCursorOnProblem;
	bool fAlreadyReportedCursorOffProblem;

	bool fAskBeforeQuit;
};

#endif
