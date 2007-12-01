#ifndef COPROCESS_H
#define COPROCESS_H

/*
   Coprocess - spawn external co-process

   Copyright (C) 2004 Matthias Reichl <hias@horus.com>

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

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

class Coprocess {
public:
	Coprocess(const char* command, int* close_fds = NULL, int close_count = 0);
	~Coprocess();

	bool WriteData(const void* buf, int len);
	bool ReadLine(char* buf, int maxlen, int& len, bool block = false);
	bool Close();
	void SetKillTimer(int timeout); // 0 to disable timer
	bool Exit(int* exitstat = NULL);
private:
	bool CheckGotLine(char* buf, int maxlen, int& len);
	void SetBlockingRead(bool block);

	enum { eReadbufSize = 1024 };
	char fReadbuf[eReadbufSize];
	int fReadbufPos;

	pid_t fChildPid;
	FILE* fWriteFile;
	int fReadFD;
	bool fIsBlocking;
	bool fIsRunning;
	bool fGotEOF;
};

#endif
