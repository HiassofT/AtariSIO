/*
   StringInput.cpp - simple line input routine for curses

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
#include <string.h>
#include <panel.h>

#include "SIOTracer.h"
#include "AtariDebug.h"

StringInput::StringInput(CursesFrontend* frontend)
	: fFrontend(frontend), fString(0), fTmpString(0)
{ }

StringInput::~StringInput()
{ }

bool StringInput::InsertChar(char c)
{
	if (fCurrentStringLength < fMaxStringLength - 1) {
		memmove(fString + fEditPos + 1, fString + fEditPos,
			fCurrentStringLength - fEditPos + 1);
		fString[fEditPos] = c;
		fEditPos++;
		fCurrentStringLength++;
		return true;
	}
	return false;
}

bool StringInput::InsertString(const char* str)
{
	if (str) {
		unsigned int len = strlen(str);
		unsigned int max = fMaxStringLength - 1 - fCurrentStringLength;
		unsigned int count = len;
		if (count > max) {
			count = max;
		}
		if (count == 0) {
			return false;
		}
		memmove(fString + fEditPos + count, fString + fEditPos,
			fCurrentStringLength - fEditPos + 1);
		memcpy(fString + fEditPos, str, count);
		fEditPos+=count;
		fCurrentStringLength+=count;
		return (count == len);
	}
	return false;
}

StringInput::ECompletionResult StringInput::TryComplete(bool)
{
	return eNoComplete;
}

bool StringInput::PrepareForSelection()
{
	return false;
}

bool StringInput::PrepareForFinish()
{
	return true;
}

StringInput::EInputResult StringInput::InputString(
	WINDOW* window,
	unsigned int xpos, unsigned int ypos, unsigned int fieldlen,
	char* string, unsigned int maxstrlen,
	int selectChar,
	bool clearOnFirstKey,
	History* history)
{
	EInputResult ret = eInputOK;

	if (maxstrlen == 0) {
		return eInputAborted;
	}

	fClearOnFirstKey = clearOnFirstKey;
	fIsFirstKey = true;
	fLastKey = -1;

	fCursesWindow = window;
	fXPos = xpos;
	fYPos = ypos;
	fFieldLength = fieldlen;
	fMaxStringLength = maxstrlen;
	if (fFieldLength >= maxstrlen) {
		fFieldLength = fMaxStringLength;
	}
	if (fString) {
		delete[] fString;
	}
	if (fTmpString) {
		delete[] fTmpString;
	}
	fString = new char[fMaxStringLength];
	fTmpString = new char[fMaxStringLength];
	fHistoryPos = -1;
	if (history) {
		fMaxHistoryPos = ((int)history->GetSize()) - 1;
	} else {
		fMaxHistoryPos = -1;
	}

	SetString(string);

	DisplayString();

	while (1) {
		//int ch = wgetch(fCursesWindow);
		int ch = fFrontend->GetCh(true);
		//DPRINTF("string input: got %d", ch);
		switch (ch) {
		case KEY_LEFT:
			if (fEditPos > 0) {
				fEditPos--;
			} else {
				beep();
			}
			break;
		case KEY_RIGHT:
			if (fEditPos < fCurrentStringLength) {
				fEditPos++;
			} else {
				beep();
			}
			break;
		case KEY_HOME:
		case KEY_FIND:
		case 1: // ^A
			fEditPos = 0;
			break;
		case KEY_END:
		case KEY_SELECT:
		case 5: // ^E
			fEditPos = fCurrentStringLength;
			break;
		case KEY_BACKSPACE:
		case 8:
			if (fEditPos > 0) {
				memmove(fString + fEditPos - 1, fString + fEditPos,
					fCurrentStringLength - fEditPos + 1);
				fCurrentStringLength--;
				fEditPos--;
			} else {
				beep();
			}
			break;
		case KEY_DC:
		case 4: // ^D
			if (fEditPos < fCurrentStringLength) {
				memmove(fString + fEditPos, fString + fEditPos + 1,
					fCurrentStringLength - fEditPos);
				fCurrentStringLength--;
			} else {
				beep();
			}
			break;
		case 21: // ^U
			fEditPos = 0;
			fCurrentStringLength = 0;
			fString[0] = 0;
			break;
		case 27: // ESC
		case 7: // ^G
			delete[] fString;
			fString = 0;
			delete[] fTmpString;
			fTmpString = 0;
			return eInputAborted;
			break;
		case 9: // TAB
			{
				bool secondComplete = false;
				if ( (!fIsFirstKey) && (fLastKey == 9) ) {
					secondComplete = true;
				}
				if (TryComplete(secondComplete) != eFullComplete) {
					beep();
				}
			}
			break;
		case KEY_UP:
			if (fHistoryPos < fMaxHistoryPos) {
				if (fHistoryPos == -1) {
					strcpy(fTmpString, fString);
				}
				fHistoryPos++;
				SetString(history->Get((unsigned int)fHistoryPos));
			} else {
				beep();
			}
			break;
		case KEY_DOWN:
			if (fHistoryPos >= 0) {
				fHistoryPos--;
				if (fHistoryPos == -1) {
					SetString(fTmpString);
				} else {
					SetString(history->Get((unsigned int)fHistoryPos));
				}
			} else {
				beep();
			}
			break;
		default:
			if (IsEndOfInputChar(ch)) {
				if (PrepareForFinish()) {
					strncpy(string, fString, maxstrlen);
					delete[] fString;
					fString = 0;
					delete[] fTmpString;
					fTmpString = 0;
					return eInputOK;
				} else {
					DPRINTF("StringInput::PrepareForFinish() failed");
					beep();
				}
			} else {
				if ( (selectChar >= 0) && (ch == selectChar)) {
					if (PrepareForSelection()) {
						ret = eInputStartSelect;
						goto end_input;
					} else {
						beep();
					}
				} else {
					if (ch >= 32 && ch <= 255) {
						if (fClearOnFirstKey && fIsFirstKey) {
							fEditPos = 1;
							fCurrentStringLength = 1;
							fString[0] = ch;
							fString[1] = 0;
						} else {
							if (!InsertChar(ch)) {
								beep();
							}
						}
					} else {
						beep();
					}
				}
			}
			break;
		}
		fLastKey = ch;
		fIsFirstKey = false;
		DisplayString();
	}

end_input:
	strncpy(string, fString, maxstrlen);
	delete[] fString;
	fString = 0;
	delete[] fTmpString;
	fTmpString = 0;
	return ret;
}

void StringInput::DisplayString()
{
	if (fEditPos < fDisplayStart) {
		fDisplayStart = fEditPos;
	}
	if (fEditPos - fDisplayStart >= fFieldLength) {
		fDisplayStart = fEditPos - fFieldLength + 1;
	}

	wmove(fCursesWindow, fYPos, fXPos);
	unsigned int plen = fCurrentStringLength - fDisplayStart;
	if (plen > fFieldLength) {
		plen = fFieldLength;
	}

	waddnstr(fCursesWindow, fString + fDisplayStart, plen);
	unsigned int i;
	for (i=plen; i<fFieldLength;i++) {
		waddch(fCursesWindow, ' ');
	}
	wmove(fCursesWindow, fYPos, fXPos + fEditPos - fDisplayStart);
	update_panels();
	doupdate();
}

void StringInput::SetString(const char* string)
{
	if (string) {
		strncpy(fString, string, fMaxStringLength);
		fString[fMaxStringLength-1] = 0;
	} else {
		fString[0] = 0;
	}
	fCurrentStringLength = strlen(fString);
	fEditPos = fCurrentStringLength;
	fDisplayStart = 0;
}

bool StringInput::IsEndOfInputChar(int ch)
{
	switch (ch) {
	case KEY_ENTER:
	case 13: // return
		return true;
	default:
		return false;
	}
}
