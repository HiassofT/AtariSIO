/*
   SearchPath - search for file in corrent directory, then in the
   specified PATH

   (c) 2004-2005 Matthias Reichl <hias@horus.com>

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

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "SearchPath.h"
#include "AtariDebug.h"
#include "OS.h"

SearchPath::SearchPath(const char* searchPath)
{
	if (searchPath) {
		fPathEntries = 1;
		int len = strlen(searchPath);
		int i;
		for (i=0;i<len;i++) {
			if (searchPath[i] == ':') {
				fPathEntries++;
			}
		}
		fPathDirectories = new char*[fPathEntries];
		int start=0, end=0, idx=0;
		while (end <= len) {
			Assert(end <= len);
			if ((searchPath[end] == ':') || (searchPath[end] == 0)) {
				Assert(idx < fPathEntries);
				if ((end-start==0) || ( (end-start==1) && (searchPath[start]=='.') ) ) {
					fPathDirectories[idx] = new char[2];
					strcpy(fPathDirectories[idx],".");
					idx++;
				} else {
					fPathDirectories[idx] = new char[end-start+1];
					strncpy(fPathDirectories[idx], searchPath + start, end - start);
					fPathDirectories[idx][end-start] = 0;
					idx++;
				}
				start = end+1;
			}
			end++;
		}
		Assert(idx == fPathEntries);
	} else {
		fPathEntries = 0;
	}
}

SearchPath::~SearchPath()
{
	if (fPathEntries) {
		int i;
		for (i=0;i<fPathEntries;i++) {
			delete[] fPathDirectories[i];
			delete[] fPathDirectories;
		}
	}
}

bool SearchPath::SearchForFile(const char* filename, char* buf, int buf_max, bool includeCWD) const
{
	struct stat statbuf;
	bool ok;
	int i;

	if (includeCWD) {
		i=-1;
	} else {
		i=0;
	}

	for (;i<fPathEntries;i++) {
		ok = false;
		if (i==-1) {
			ok = (snprintf(buf, buf_max, "%s", filename) != -1);
			Assert(ok);
		} else {
			if (fPathDirectories[i][0]) {
				ok = (snprintf(buf, buf_max, "%s%c%s", fPathDirectories[i], DIR_SEPARATOR, filename) != -1);
			} else {
				Assert(false);
			}
		}
		if (ok) {
			if (stat(buf, &statbuf) == 0) {
				//DPRINTF("match: %s", buf);
				return true;
			}
		}
	}
	return false;
}
