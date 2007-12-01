#ifndef HISTORY_H
#define HISTORY_H

/*
   History.h - input history class

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

class History {
public:
	History(unsigned int maxsize = 100);
	~History();

	void Add(const char* string);
	const char* Get(unsigned int num);
	unsigned int GetSize() { return fSize; }

private:
	class HistoryEntry : public RefCounted {
	public:
		HistoryEntry(const char* string);
		virtual ~HistoryEntry();
		
		char* fString;
		RCPtr<HistoryEntry> fNext;
		HistoryEntry* fPrev;
	};

	unsigned int fMaxSize;
	unsigned int fSize;

	RCPtr<HistoryEntry> fFirst;
	RCPtr<HistoryEntry> fLast;
};
#endif
