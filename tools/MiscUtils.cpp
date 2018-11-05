/*
   MiscUtils.cpp - misc helper routines

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
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <limits.h>

#include "OS.h"
#include "MiscUtils.h"
#include "AtariDebug.h"
#include "Directory.h"

#if !defined(WINVER) && !defined(POSIXVER)
#include <sched.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <time.h>
#endif

#include "winver.h"

using namespace MiscUtils;

char* MiscUtils::ShortenFilename(const char* filename, unsigned int maxlen, bool stripExtension)
{
	if (filename == NULL) {
		return NULL;
	}

	if (maxlen == 0) {
		return NULL;
	}
	unsigned int len = strlen(filename);
	char * s = new char[maxlen+1];

	const char *p = NULL;
	const char *pd = NULL;

	char * tmpstr = strdup(filename);

	if (stripExtension) {
		char * dotpos = strrchr(tmpstr, '.');
		if (dotpos) {
			unsigned int dotidx = dotpos - tmpstr;
			if (len - dotidx <= 4) {
				tmpstr[dotidx] = 0;
			}
		}
	}
	len = strlen(tmpstr);

	if (len<=maxlen) {
		strcpy(s, tmpstr);
		free(tmpstr);
		return s;
	}
	if (maxlen <= 3) {
		const char *fn = strrchr(tmpstr, DIR_SEPARATOR);
		if (fn == NULL) {
			strncpy(s, tmpstr, maxlen);
		} else {
			strncpy(s, fn + 1, maxlen);
		}
		s[maxlen] = 0;
		free(tmpstr);
		return s;
	}
	p = tmpstr+len-maxlen+3;
	pd = p;
	pd = strchr(p, DIR_SEPARATOR);

	// TODO: correctly display directories with trailing slash

	if ((pd != NULL) && (pd != tmpstr+len-1) ) {
		strncpy(s, "...", 3);
		strncpy(s + 3, pd, maxlen - 2);
		free(tmpstr);
		return s;
	}
	while (len && (tmpstr[len-1] == DIR_SEPARATOR)) {
		tmpstr[--len] = 0;
	}
	pd = strrchr(tmpstr, DIR_SEPARATOR);
	if (pd) {
		p = pd+1;
	} else {
		p = tmpstr;
	}
	strncpy(s, p, maxlen);
	s[maxlen] = 0;
	free(tmpstr);
	return s;
}

// format is "divisor[,baudrate]"
bool MiscUtils::ParseHighSpeedParameters(const char* string, uint8_t& divisor, unsigned int& baudrate)
{
	char* tmp;
	long l;
	baudrate = 0;
	l = strtol(string, &tmp, 10);
	if (tmp == string) {
		return false;
	}
	if (l < 0 || l > 63) {
		return false;
	}
	divisor = l;

	if (*tmp == 0) {
		return true;
	}
	if (*tmp != ',') {
		return false;
	}
	string = tmp+1;
	l = strtol(string, &tmp, 10);
	if (tmp == string) {
		return false;
	}
	if (l < 0 || l > 150000) {
		return false;
	}
	baudrate = l;
	if (*tmp != 0) {
		return false;
	}
	return true;
}

#if !defined(WINVER) && !defined(POSIXVER)
static void reserve_stack_memory()
{
#define RESERVE_STACK_SIZE 100000
	char dummy_string[RESERVE_STACK_SIZE];
	memset(dummy_string, 0, RESERVE_STACK_SIZE);
}

static bool uids_set = false;

static uid_t euid, uid;
static gid_t egid, gid;

bool MiscUtils::drop_root_privileges()
{
        euid = geteuid();
	uid = getuid();
	egid = getegid();
	gid = getgid();

	// drop root privileges
	if (seteuid(uid) != 0) {
		printf("cannot set euid to %d\n", uid);
		exit(1);
	}
	if (setegid(gid) != 0) {
		printf("cannot set egid to %d", gid);
		exit(1);
	}
	uids_set = true;
	return true;
}

static bool realtime_sched_set = false;
static int old_sched_policy;
static int old_sched_priority;

bool MiscUtils::set_realtime_scheduling(int priority)
{
	struct sched_param sp;
	pid_t myPid;

	bool ret = false;

	if (uids_set) {
		if (seteuid(euid) != 0) {
			printf("cannot set euid back to %d\n", euid);
			exit(1);
		}
		if (setegid(egid) != 0) {
			printf("cannot set egid back to %d\n", egid);
			exit(1);
		}
	}

	myPid = getpid();

	old_sched_policy = sched_getscheduler(myPid);
	if (sched_getparam(myPid, &sp) != 0) {
		printf("cannot get scheduler parameters\n");
		exit(1);
	}
	old_sched_priority = sp.sched_priority;

	memset(&sp, 0, sizeof(struct sched_param));
	sp.sched_priority = sched_get_priority_max(SCHED_RR) - priority;

	if (sched_setscheduler(myPid, SCHED_RR, &sp) == 0) {
		realtime_sched_set = true;
		ret = true;
		ALOG("activated realtime scheduling");
	} else {
		AWARN("Cannot set realtime scheduling! please run as root!");
	}

	reserve_stack_memory();
	if (mlockall(MCL_CURRENT) == 0) {
		ALOG("mlockall(2) succeeded");
	} else {
		AWARN("mlockall(2) failed!");
	}

	if (uids_set) {
		// drop root privileges - set eid to real id
		if (seteuid(uid) != 0) {
			printf("cannot set euid to %d\n", uid);
			exit(1);
		}
		if (setegid(gid) != 0) {
			printf("cannot set egid to %d\n", gid);
			exit(1);
		}
	}
	return ret;
}

bool MiscUtils::drop_realtime_scheduling()
{
	if (!uids_set) {
		Assert(false);
		exit(1);
	}
	if (realtime_sched_set) {
		// acquire root privileges
		if (seteuid(euid) != 0) {
			printf("cannot set euid back to %d\n", euid);
			return false;
		}
		if (setegid(egid) != 0) {
			printf("cannot set egid back to %d\n", egid);
			return false;
		}

		pid_t myPid = getpid();
		struct sched_param sp;
		memset(&sp, 0, sizeof(struct sched_param));
		sp.sched_priority = old_sched_priority;

		if (sched_setscheduler(myPid, SCHED_OTHER, &sp)) {
			printf("error setting back standard scheduler\n");
			return false;
		}

		if (seteuid(uid) != 0) {
			printf("cannot set euid to %d\n", uid);
			return false;
		}
		if (setegid(gid) != 0) {
			printf("cannot set egid to %d\n", gid);
			return false;
		}
	}
	return true;
}

#define NANOSLEEP_THRES 20000

void MiscUtils::WaitUntil(TimestampType endTime)
{
	TimestampType startTime = GetCurrentTime();
	if (startTime > endTime) {
/*
		DPRINTF("WaitUntil called with endTime < current Time (%lld diff)",
			startTime-endTime);
*/
		return;
	}

	TimestampType diff = endTime - startTime;
	TimestampType currentTime;

	if (diff > NANOSLEEP_THRES ) {
		struct timespec ts;
		ts.tv_sec = (diff-NANOSLEEP_THRES)/1000000;
		ts.tv_nsec = ((diff-NANOSLEEP_THRES) % 1000000)*1000;
		nanosleep(&ts, NULL);
	}
	do {
		currentTime = GetCurrentTime();
	} while (currentTime >= startTime && currentTime < endTime);

/*
	if (currentTime > endTime+100) {
		DPRINTF("WaitUntil waited %lld usec too long!",
		currTime-endTime);
	}
*/
}
#endif

void MiscUtils::ByteToFsk(const uint8_t byte, std::list<uint16_t>& bit_delays, unsigned int bit_time)
{
	int current_bit = 0;
	int delay = bit_time; // account for start bit;

	int i;
	int mybyte = byte | 0x100;	// byte plus stop-bit

	for (i=0; i<9 ;i++) {
		if ( (mybyte & 1) == current_bit) {
			delay += bit_time;
		} else {
			bit_delays.push_back(delay);
			current_bit = mybyte & 1;
			delay = bit_time;
		}
		mybyte >>= 1;
	}
	bit_delays.push_back(delay);
}

bool MiscUtils::DataBlockToFsk(const uint8_t* data, unsigned int data_len, uint16_t** fsk_data, unsigned int* fsk_len)
{
	if (data == 0 || data_len == 0 || fsk_data == 0 || fsk_len == 0) {
		return false;
	}
        unsigned int i;
        std::list<uint16_t> fsk;
        for (i=0; i<data_len; i++) {
                MiscUtils::ByteToFsk(data[i], fsk);
        }
        *fsk_len = fsk.size();
        *fsk_data = new uint16_t[*fsk_len];

        std::list< uint16_t >::const_iterator iter;
        std::list< uint16_t >::const_iterator listend = fsk.end();
        i = 0;
        for (iter=fsk.begin(); iter != listend; iter++) {
                (*fsk_data)[i++] = *iter;
        }
	return true;
}

