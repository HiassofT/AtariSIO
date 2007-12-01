/*
   Coprocess - 

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

#include "Coprocess.h"
#include "MiscUtils.h"
#include "AtariDebug.h"
#include "Error.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

static pid_t kill_pid = 0;

static void coprocess_sig_handler(int sig)
{
	if ((sig == SIGALRM) && (kill_pid != 0)) {
		kill(kill_pid, SIGKILL);
	}
}

void Coprocess::SetBlockingRead(bool block)
{
	if (block != fIsBlocking) {
		int val;
		if (fcntl(fReadFD, F_GETFL, &val) >= 0) {
			if (block) {
				val &= ~(O_NONBLOCK);
			} else {
				val |= O_NONBLOCK;
			}
			fcntl(fReadFD, F_SETFL, val);
		}
		fIsBlocking = block;
	}
}

Coprocess::Coprocess(const char* command, int* close_fds, int close_count)
{
	int write_fd[2], read_fd[2];
	pid_t	pid;

	if (pipe(read_fd) < 0) {
		throw ErrorObject("cannot create pipe");
	}

	if (pipe(write_fd) < 0) {
		close(read_fd[0]);
		close(read_fd[1]);
		throw ErrorObject("cannot create pipe");
	}

	if ( (pid = fork()) < 0) {
		close(read_fd[0]);
		close(read_fd[1]);
		close(write_fd[0]);
		close(write_fd[1]);
		throw ErrorObject("cannot fork");
	} else if (pid == 0) {	// child
		if (!MiscUtils::drop_realtime_scheduling()) {
			_exit(1);
		}
		if (write_fd[0] != STDIN_FILENO) {
			dup2(write_fd[0], STDIN_FILENO);
		}
		if (read_fd[1] != STDOUT_FILENO) {
			dup2(read_fd[1], STDOUT_FILENO);
		}
		if (read_fd[1] != STDERR_FILENO) {
			dup2(read_fd[1], STDERR_FILENO);
		}
		close(read_fd[0]);
		close(read_fd[1]);
		close(write_fd[0]);
		close(write_fd[1]);
		if (close_fds && (close_count > 0)) {
			int i;
			for (i=0;i<close_count;i++) {
				close(close_fds[i]);
			}
		}

		execl("/bin/sh", "sh", "-c", command, (char *) 0);
		_exit(127);
	}
	/* parent */
	fChildPid = pid;
	close(write_fd[0]);
	close(read_fd[1]);

	fReadFD = read_fd[0];

	fIsBlocking = true;
	SetBlockingRead(false);

	fWriteFile = fdopen(write_fd[1], "w");
	if (!fWriteFile) {
		close(fReadFD);
		close(write_fd[1]);
		throw ErrorObject("cannot open write pipe");
	}
	if (fflush(fWriteFile)) {
		close(fReadFD);
		fclose(fWriteFile);
		throw ErrorObject("cannot open write pipe");
	}

	fReadbufPos = 0;
	fGotEOF = false;
	fIsRunning = true;
}

Coprocess::~Coprocess()
{
	if (fIsRunning) {
		Close();
		Exit();
		Assert(false);
	}
}

bool Coprocess::Close()
{
	bool ret = true;
	if (!fIsRunning) {
		Assert(false);
		return false;
	}

	if (fWriteFile) {
		fflush(fWriteFile);
		if (fclose(fWriteFile)) {
			AERROR("cannot close coprocess write pipe");
			ret = false;
		}
		fWriteFile = NULL;
	} else {
		ret = false;
		Assert(false);
	}
	return ret;
}

bool Coprocess::Exit(int* exitstat)
{
	bool ret = true;
	if (!fIsRunning) {
		Assert(false);
		return false;
	}

	int pid, stat;
	if (fReadFD >= 0) {
		if (close(fReadFD)) {
			AERROR("cannot close coprocess read pipe");
			ret = false;
		}
		fReadFD = -1;
	} else {
		ret = false;
		Assert(false);
	}

	while ((pid=waitpid(fChildPid, &stat, 0)) < 0) {
		if (errno != EINTR) {
			AERROR("waiting for coprocess termination failed");
			ret = false;
		}
	}
	if (exitstat) {
		*exitstat = stat;
	}
	fIsRunning = false;
	return ret;
}

bool Coprocess::WriteData(const void* buf, int len)
{
	if (!fIsRunning) {
		Assert(false);
		return false;
	}
	if (fWriteFile == 0) {
		Assert(false);
		return false;
	}
	if (len == 0) {
		AWARN("call to write_coprocess with len 0");
		return true;
	}
	int l;
	if ((l=fwrite(buf, 1, len, fWriteFile)) != len) {
		//ALOG("write returned %d\n", l);
		return false;
	}
	if (fflush(fWriteFile)) {
		//ALOG("error in fflush");
		return false;
	}
	return true;
}

bool Coprocess::CheckGotLine(char* buf, int maxlen, int& len)
{
	if (fReadbufPos) {
		char* eol=(char*)memchr(fReadbuf, '\n', fReadbufPos);
		if (eol) {
			len = (size_t) eol - (size_t) fReadbuf;
			int linelen = len;
			if (linelen) {
				if (linelen > maxlen - 1) {
					linelen = maxlen - 1;
					len = maxlen - 1;
				} else {
					linelen++;
				}
				memcpy(buf, fReadbuf, len);
			} else {
				linelen++;
			}
			buf[len] = 0;
			if (linelen < fReadbufPos) {
				memmove(fReadbuf, fReadbuf + linelen, fReadbufPos - linelen);
			}
			fReadbufPos -= linelen;
			return true;
		} else {
			if (fGotEOF || (fReadbufPos == eReadbufSize) || (fReadbufPos >= maxlen - 1) ) {
				len = fReadbufPos;
				if (len == 0) {
					return false;
				}
				if (len > maxlen - 1) {
					len = maxlen - 1;
				}
				memcpy(buf, fReadbuf, len);
				buf[len] = 0;
				if (len < fReadbufPos) {
					memmove(fReadbuf, fReadbuf + len, fReadbufPos - len);
				}
				fReadbufPos -= len;
				return true;
			}
		}
	}
	return false;
}

bool Coprocess::ReadLine(char* buf, int maxlen, int& len, bool block)
{
	if (!fIsRunning) {
		Assert(false);
		return false;
	}
	if (fReadFD < 0) {
		Assert(false);
		return false;
	}
	if (maxlen <=1) {
		Assert(false);
		return false;
	}
	if (CheckGotLine(buf, maxlen, len)) {
		return true;
	}
	if (fGotEOF) {
		Assert(fReadbufPos == 0);
		return false;
	}

	SetBlockingRead(block);

	int remain = eReadbufSize - fReadbufPos;
	int read_len=read(fReadFD, fReadbuf + fReadbufPos, remain);
	if (read_len < 0) {
		return false;
	} else if (read_len == 0) {
		if (block) {
			fGotEOF = true;
		}
	} else {
		fReadbufPos += read_len;
	}
	return CheckGotLine(buf, maxlen, len);
}

void Coprocess::SetKillTimer(int timeout)
{
	
	if (timeout > 0) {
		kill_pid = fChildPid;
		if (signal(SIGALRM, coprocess_sig_handler) == SIG_ERR) {
			ALOG("error setting signal handler");
		}
		alarm(timeout);
	} else {
		alarm(0);
		kill_pid=0;
		signal(SIGALRM, SIG_DFL);
	}
}
