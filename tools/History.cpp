/*
   History.cpp - input history class

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
#include <string.h>

History::History(unsigned int maxsize)
	: fMaxSize(maxsize),
	  fSize(0),
	  fFirst(0),
	  fLast(0)
{
	if (fMaxSize == 0) {
		fMaxSize = 1;
	}
}

History::~History()
{
	fLast = 0;
	fFirst = 0;
}

History::HistoryEntry::HistoryEntry(const char* string)
	: fNext(0), fPrev(0)
{
	int len = strlen(string);
	fString = new char[len + 1];
	strcpy(fString, string);
}

History::HistoryEntry::~HistoryEntry()
{
	delete[] fString;
	fString = 0;
}

void History::Add(const char* string)
{
	if (string == 0) {
		return;
	}
	if (*string == 0) {
		return;
	}
	RCPtr<HistoryEntry> entry = new HistoryEntry(string);
	if (fSize == 0) {
		fFirst = entry;
		fLast = entry;
		fSize = 1;
	} else {
		entry->fNext = fFirst;
		if (fFirst) {
			fFirst->fPrev = entry.GetRealPointer();
		}
		fFirst = entry;

		if (fSize == fMaxSize) {
			fLast = fLast->fPrev;
			fLast->fNext = 0;
		} else {
			fSize++;
		}
	}
}

const char* History::Get(unsigned int num)
{
	if (num >= fSize) {
		return 0;
	}
	RCPtr<HistoryEntry> entry = fFirst;
	while (num--) {
		entry = entry->fNext;
	}
	return entry->fString;
}
