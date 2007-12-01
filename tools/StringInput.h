#ifndef STRINGINPUT_H
#define STRINGINPUT_H

/*
   StringInput.h - simple line input routine for curses

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

#include "History.h"
#include "CursesFrontend.h"

#include <curses.h>

class StringInput {
public:
	StringInput(CursesFrontend* frontend);
	virtual ~StringInput();

	enum EInputResult {
		eInputOK = 0,
		eInputAborted = 1,
		eInputStartSelect = 2
	};

	EInputResult InputString(
		WINDOW* window,
		unsigned int xpos, unsigned int ypos, unsigned int fieldlen,
		char* string, unsigned int maxstrlen,
		int selectChar = -1,
		bool clearOnFirstKey = false,
		History* history = 0);

	enum ECompletionResult {
		eNoComplete,
		ePartialComplete,
		eFullComplete
	};
	virtual bool PrepareForSelection();
	virtual bool PrepareForFinish();

protected:
	virtual bool IsEndOfInputChar(int ch);
	virtual ECompletionResult TryComplete(bool lastActionWasAlsoComplete);

	void SetString(const char* string);

	void DisplayString();

	bool InsertChar(char c);
	bool InsertString(const char* string);

	CursesFrontend* fFrontend;

	WINDOW *fCursesWindow;
	unsigned int fXPos, fYPos, fFieldLength;
	unsigned int fDisplayStart;
	unsigned int fEditPos;
	char* fString;
	char* fTmpString;
	unsigned int fMaxStringLength;
	unsigned int fCurrentStringLength;

	bool fClearOnFirstKey;
	bool fIsFirstKey;

	int fHistoryPos;
	int fMaxHistoryPos;

	int fLastKey;
};

#endif
