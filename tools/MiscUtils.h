#ifndef MISCUTILS_H
#define MISCUTILS_H

/*
   MiscUtils.h - misc helper routines

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

#include <stdio.h>
#include <stdint.h>
#include "DiskImage.h"
#include "Dos2xUtils.h"
#include <list>
#ifndef WINVER
#include <sys/time.h>
#endif

namespace MiscUtils {
	char* ShortenFilename(const char* filename, unsigned int maxlen, bool stripExtension = false);

#if !defined(WINVER) && !defined(POSIXVER)

	bool drop_root_privileges();
	bool set_realtime_scheduling(int priority);
	bool drop_realtime_scheduling();

	typedef uint64_t TimestampType;

	inline TimestampType TimevalToTimestamp(struct timeval& tv)
	{
		return (TimestampType)tv.tv_sec * 1000000 + tv.tv_usec;
	}

	inline TimestampType UsecToTimestamp(unsigned long usec)
	{
		return (TimestampType) usec;
	}

	inline TimestampType MsecToTimestamp(unsigned long msec)
	{
		return (TimestampType) msec * 1000;
	}

	inline TimestampType SecToTimestamp(unsigned long sec)
	{
		return (TimestampType) sec * 1000000;
	}
	
	inline void TimestampToTimeval(TimestampType ts, struct timeval& tv)
	{
		tv.tv_sec = ts / 1000000;
		tv.tv_usec = ts % 1000000;
	}

	inline TimestampType GetCurrentTime()
	{
		struct timeval tv;
		gettimeofday(&tv, NULL);
		return TimevalToTimestamp(tv);
	}

	inline TimestampType GetCurrentTimePlusUsec(unsigned long usec)
	{
		return GetCurrentTime() + UsecToTimestamp(usec);
	}

	inline TimestampType GetCurrentTimePlusMsec(unsigned long msec)
	{
		return GetCurrentTime() + MsecToTimestamp(msec);
	}

	inline TimestampType GetCurrentTimePlusSec(unsigned long sec)
	{
		return GetCurrentTime() + SecToTimestamp(sec);
	}

	void WaitUntil(TimestampType endTime);

#endif

	inline void EatSpace(const char* &string)
	{
		if (string) {
			while (*string == ' ') {
				string++;
			}
		}
	}

	// baudrate will be zero if optional baudrate isn't set in string
	bool ParseHighSpeedParameters(const char* string, uint8_t& pokeyDivisor, unsigned int& baudrate);

	void ByteToFsk(const uint8_t byte, std::list<uint16_t>& bit_delays, unsigned int bit_time = 16);

	bool DataBlockToFsk(const uint8_t* data, unsigned int data_len, uint16_t** fsk_data, unsigned int* fsk_len);

};


#endif
