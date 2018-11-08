/*
   measure-system-latency: measure serial transmission timing parameters

   Copyright (C) 2006 Matthias Reichl <hias@horus.com>

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

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>

#include "AtrMemoryImage.h"
#include "SIOWrapper.h"
#include "KernelSIOWrapper.h"
#include "SIOTracer.h"
#include "FileTracer.h"
#include "Error.h"
#include "MiscUtils.h"

using namespace MiscUtils;

#include "Version.h"

#define MAX_BLOCKSIZE 8192

static RCPtr<SIOWrapper> SIO;
static SIOTracer* sioTracer = 0;

static void my_sig_handler(int sig)
{
	switch (sig) {
	case SIGALRM:
	case SIGINT:
	case SIGTERM:
	case SIGHUP:
		if (sioTracer) {
			sioTracer->RemoveAllTracers();
		}
		exit(0);
	}
}

typedef struct my_timestamp_struct {
	TimestampType start;
	SIO_timestamps sio;
	TimestampType end;
} MyTimestamps;

static void calc_relative_timestamps(MyTimestamps& ts)
{
	ts.sio.system_entering        -= ts.start;
	ts.sio.transmission_start     -= ts.start;
	ts.sio.transmission_send_irq  -= ts.start;
	ts.sio.transmission_end       -= ts.start;
	ts.sio.transmission_wakeup    -= ts.start;
	ts.sio.uart_finished          -= ts.start;
	ts.sio.system_leaving         -= ts.start;
	ts.end                        -= ts.start;
}

static MyTimestamps min_ts, max_ts, avg_ts;

static void check_ts_min(MyTimestamps& ts)
{
	if (ts.sio.system_entering < min_ts.sio.system_entering) {
		min_ts.sio.system_entering = ts.sio.system_entering;
	}
	if (ts.sio.transmission_start < min_ts.sio.transmission_start) {
		min_ts.sio.transmission_start = ts.sio.transmission_start;
	}
	if (ts.sio.transmission_send_irq < min_ts.sio.transmission_send_irq) {
		min_ts.sio.transmission_send_irq = ts.sio.transmission_send_irq;
	}
	if (ts.sio.transmission_end < min_ts.sio.transmission_end) {
		min_ts.sio.transmission_end = ts.sio.transmission_end;
	}
	if (ts.sio.transmission_wakeup < min_ts.sio.transmission_wakeup) {
		min_ts.sio.transmission_wakeup = ts.sio.transmission_wakeup;
	}
	if (ts.sio.uart_finished < min_ts.sio.uart_finished) {
		min_ts.sio.uart_finished = ts.sio.uart_finished;
	}
	if (ts.sio.system_leaving < min_ts.sio.system_leaving) {
		min_ts.sio.system_leaving = ts.sio.system_leaving;
	}
	if (ts.end < min_ts.end) {
		min_ts.end = ts.end;
	}
}

static void check_ts_max(MyTimestamps& ts)
{
	if (ts.sio.system_entering > max_ts.sio.system_entering) {
		max_ts.sio.system_entering = ts.sio.system_entering;
	}
	if (ts.sio.transmission_start > max_ts.sio.transmission_start) {
		max_ts.sio.transmission_start = ts.sio.transmission_start;
	}
	if (ts.sio.transmission_send_irq > max_ts.sio.transmission_send_irq) {
		max_ts.sio.transmission_send_irq = ts.sio.transmission_send_irq;
	}
	if (ts.sio.transmission_end > max_ts.sio.transmission_end) {
		max_ts.sio.transmission_end = ts.sio.transmission_end;
	}
	if (ts.sio.transmission_wakeup > max_ts.sio.transmission_wakeup) {
		max_ts.sio.transmission_wakeup = ts.sio.transmission_wakeup;
	}
	if (ts.sio.uart_finished > max_ts.sio.uart_finished) {
		max_ts.sio.uart_finished = ts.sio.uart_finished;
	}
	if (ts.sio.system_leaving > max_ts.sio.system_leaving) {
		max_ts.sio.system_leaving = ts.sio.system_leaving;
	}
	if (ts.end > max_ts.end) {
		max_ts.end = ts.end;
	}
}

/*
static void init_ts_avg()
{
	avg_ts.sio.system_entering = 0;
	avg_ts.sio.transmission_start = 0;
	avg_ts.sio.transmission_send_irq = 0;
	avg_ts.sio.transmission_end = 0;
	avg_ts.sio.transmission_wakeup = 0;
	avg_ts.sio.uart_finished = 0;
	avg_ts.sio.system_leaving = 0;
	avg_ts.end = 0;
}
*/

static void add_ts_avg(MyTimestamps& ts)
{
	avg_ts.sio.system_entering += ts.sio.system_entering;
	avg_ts.sio.transmission_start += ts.sio.transmission_start;
	avg_ts.sio.transmission_send_irq += ts.sio.transmission_send_irq;
	avg_ts.sio.transmission_end += ts.sio.transmission_end;
	avg_ts.sio.transmission_wakeup += ts.sio.transmission_wakeup;
	avg_ts.sio.uart_finished += ts.sio.uart_finished;
	avg_ts.sio.system_leaving += ts.sio.system_leaving;
	avg_ts.end += ts.end;
}

static void avg_ts_divide(unsigned int count)
{
	if (!count) {
		return;
	}
	avg_ts.sio.system_entering /= count;
	avg_ts.sio.transmission_start /= count;
	avg_ts.sio.transmission_send_irq /= count;
	avg_ts.sio.transmission_end /= count;
	avg_ts.sio.transmission_wakeup /= count;
	avg_ts.sio.uart_finished /= count;
	avg_ts.sio.system_leaving /= count;
	avg_ts.end /= count;
}

static void print_ts(MyTimestamps& ts)
{
	printf("%6" PRIu64 " %6" PRIu64 " %6" PRIu64 " %6" PRIu64 " %6" PRIu64 " %6" PRIu64 " %6" PRIu64 " %6" PRIu64,
		ts.sio.system_entering,
		ts.sio.transmission_start,
		ts.sio.transmission_send_irq,
		ts.sio.transmission_end,
		ts.sio.transmission_wakeup,
		ts.sio.uart_finished,
		ts.sio.system_leaving,
		ts.end);
}

static void print_ts_info()
{
	//      123456 123456 123456 123456 123456 123456 123456 123456
	printf(" enter xstart   xirq   xend wakeup finish  leave    end");
}

enum EOutputMode {
	eOutputNone,
	eOutputSummary,
	eOutputFull,
	eOutputGnuplot
};

// set blocksize to 0..1024 for raw send timing measurement,
// setting it to a negative value means time ioctl latency only

static unsigned long measure_latency(int blocksize, unsigned int count,  EOutputMode outputMode, unsigned int baudrate=19200)
{
	if (blocksize > MAX_BLOCKSIZE) {
		fprintf(stderr, "illegal blocksize %d\n", blocksize);
		return 0;
	}
	if (!count) {
		fprintf(stderr, "count must not be 0");
		return 0;
	}

	uint8_t buf[MAX_BLOCKSIZE];
	memset(buf, 0, MAX_BLOCKSIZE);

	MyTimestamps ts;

	unsigned int i;
	unsigned int good_count = 0;
	int ret;

	switch (outputMode) {
	case eOutputFull:
		printf("\n");
		// fallthrough
	case eOutputSummary:
		if (blocksize >= 0) {
			printf("%4d bytes:", blocksize);
		} else {
			printf("     ioctl:");
		}
		break;
	default: break;
	}

	if (outputMode == eOutputFull) {
		printf("\ncount ");
		print_ts_info();
		printf("\n");
	}
	fflush(stdout);

	for (i=0;i<count;i++) {
		if (outputMode != eOutputNone) {
			fflush(stdout);
		}
		ts.start = GetCurrentTime();

		if (blocksize >= 0) {
			ret = SIO->SendRawFrame(buf, blocksize);
		} else {
			if (SIO->IsKernelWrapper()) {
				RCPtr<KernelSIOWrapper> ksio = RCPtrStaticCast<KernelSIOWrapper>(SIO);
				ret = ksio->GetKernelDriverVersion();
				if (ret > 0) {
					ret = 0;
				}
			} else {
				ret = -EINVAL;
			}
		}
		if (ret) {
			fprintf(stderr, "ioctl failed!\n");
		} else {
			ts.end = GetCurrentTime();
			if (SIO->GetTimestamps(ts.sio)) {
				fprintf(stderr, "error reading timestamps\n");
			} else {
				calc_relative_timestamps(ts);
				if (outputMode == eOutputFull) {
					printf("%5d ", good_count);
					print_ts(ts);
					printf("\n");
				}
				if (good_count) {
					check_ts_min(ts);
					check_ts_max(ts);
					add_ts_avg(ts);
				} else {
					min_ts = ts;
					max_ts = ts;
					avg_ts = ts;
				}
				good_count++;
			}
		}
	}
	avg_ts_divide(good_count);
	if (outputMode == eOutputFull) {
		printf("\ntiming statistics:");
		printf("\n  min "); print_ts(min_ts);
		printf("\n  avg "); print_ts(avg_ts);
		printf("\n  max "); print_ts(max_ts);
		printf("\n");
	}
	TimestampType calculatedEndTime = 0;
	if (blocksize > 0) {
       		calculatedEndTime = ((TimestampType)blocksize * 10 * 1000 * 1000) / baudrate;
	}

	TimestampType realEndTime, realTransmissionTime;

	if (blocksize > 0) {
		realEndTime = avg_ts.sio.uart_finished;
		realTransmissionTime = avg_ts.sio.uart_finished - avg_ts.sio.transmission_send_irq;
	} else {
		realEndTime = avg_ts.end;
		realTransmissionTime = avg_ts.end;
	}

	switch (outputMode) {
	case eOutputGnuplot:
		//printf("%d %lld", blocksize, realEndTime);
		printf("%d %" PRIu64, blocksize, realTransmissionTime);
		break;
	case eOutputFull:
		printf("\navg. xmit time:");
		// fallthrough
	case eOutputSummary:
		printf(" %7" PRIu64 " usec", realTransmissionTime);
		//printf(" %7" PRIu64 " usec", realEndTime);
		break;
	default: break;
	}

	if (calculatedEndTime) {
		//signed long timeDiff = (signed long)realEndTime - (signed long)calculatedEndTime;
		signed long timeDiff = (signed long)realTransmissionTime - (signed long)calculatedEndTime;
		double error = ((double)timeDiff / (double)calculatedEndTime) * 100.0;
		switch (outputMode) {
		case eOutputSummary:
		case eOutputFull:
			printf (" (calculated: %7" PRIu64 "  error: %4ld usec / %7.3f%%)", calculatedEndTime, timeDiff, error);
			break;
		default: break;
		}
	}
	if (outputMode != eOutputNone) {
		printf("\n");
	}

	return realEndTime;
}

bool CalcTiming(unsigned long t1, unsigned long t2,
		unsigned int b1, unsigned int b2,
		long& intLatency, long& intSpeed)
{
	if (t1 == t2) {
		fprintf(stderr, "cannot calculate latency!\n");
		return false;
	}
	double usecPerByte = (double)(t2 - t1) / (b2 - b1);
	double latency = t2 - usecPerByte * b2;
	double bitsPerSecond = 10000000.0/usecPerByte;

	intLatency = (long) (latency+0.5);
	intSpeed = (long) (bitsPerSecond+0.5);

	return true;
}


int main(int argc, char** argv)
{
	sioTracer = SIOTracer::GetInstance();
	RCPtr<FileTracer> tracer(new FileTracer(stderr));
	sioTracer->AddTracer(tracer);

	sioTracer->SetTraceGroup(SIOTracer::eTraceInfo, true, tracer);
	sioTracer->SetTraceGroup(SIOTracer::eTraceWarning, true, tracer);
	sioTracer->SetTraceGroup(SIOTracer::eTraceError, true, tracer);
	sioTracer->SetTraceGroup(SIOTracer::eTraceDebug, true, tracer);
	sioTracer->SetTraceGroup(SIOTracer::eTraceVerboseCommands, true, tracer);

	EOutputMode outputMode = eOutputSummary;
	bool calcLatencyParameters = false;
	bool highspeedMode = false;
	bool slowMode = false;

	const char* siodev = 0;

	unsigned int highbaud = 57600;

	int c;
	while ((c = getopt(argc, argv, "vglhsd:b:")) > 0) {
		switch(c) {
		case 'b': 
			highbaud = atoi(optarg);
			printf("using highspeed baudrate %d\n", highbaud);
			break;
		case 'd': 
			siodev = optarg;
			printf("using device %s\n", siodev);
			break;
		case 'v': 
			outputMode = eOutputFull;
			break;
		case 'g': 
			outputMode = eOutputGnuplot;
			break;
		case 'l':
			calcLatencyParameters = true;
			break;
		case 'h': 
			highspeedMode = true;
			break;
		case 's':
			slowMode = true;
			break;
		default: break;
		}
	}

	try {
		SIO = SIOWrapper::CreateSIOWrapper(siodev);
	}
	catch (ErrorObject& err) {
		std::cerr << err.AsString() << std::endl;
		return 1;
	}
       	if (MiscUtils:: set_realtime_scheduling(0)) {
#ifdef ATARISIO_DEBUG
		ALOG("this program will automatically terminate in 5 minutes!");
		signal(SIGALRM, my_sig_handler);
		alarm(300);
#endif
	}

	if (SIO->Set1050CableType(SIOWrapper::e1050_2_PC)) {
		printf("cannot set cable type to 1050-2-pc\n");
		return 1;
	}

	if (SIO->EnableTimestampRecording(1)) {
		printf("cannot enable timestamp recording\n");
		return 1;
	}

	if (slowMode) {
		SIO->SetHighSpeedPause(ATARISIO_HIGHSPEEDPAUSE_BYTE_DELAY);
		printf("enabled high speed pause\n");
	}

	unsigned int baudrate = 19200;
	if (highspeedMode) {
		baudrate = highbaud;
	}

	if (SIO->SetBaudrate(baudrate)) {
		printf("cannot set baudrate\n");
		return 1;
	}

	if (calcLatencyParameters) {
		// calculate latency and transmission speed
		unsigned int b1 = 130;
		unsigned int b2 = 258;

		unsigned int count = 50;

		unsigned long t1 = measure_latency(b1, count, eOutputNone, baudrate);
		unsigned long t2 = measure_latency(b2, count, eOutputNone, baudrate);

		long intLatency;
		long intSpeed;

		if (CalcTiming(t1, t2, b1, b2, intLatency, intSpeed)) {
			printf("latency parameters: %ld usec speed: %ld bits/sec\n", intLatency, intSpeed);
			printf("set AtariSIO timing with \"export ATARISIO_TIMING=%ld,%ld\"\n", intLatency, intSpeed);
		} else {
			printf("calculation of latency parameters failed!\n");
		}
	} else {
		if (outputMode != eOutputGnuplot) {
			printf("\n");
		}

		unsigned int count = 10;

		if (optind==argc) {
			measure_latency(  -1, count, outputMode, baudrate);
			measure_latency(   0, count, outputMode, baudrate);
			measure_latency(   1, count, outputMode, baudrate);
			measure_latency(   2, count, outputMode, baudrate);
			measure_latency(   3, count, outputMode, baudrate);
			measure_latency(   4, count, outputMode, baudrate);
			measure_latency( 128, count, outputMode, baudrate);
			measure_latency( 129, count, outputMode, baudrate);
			measure_latency( 256, count, outputMode, baudrate);
			measure_latency( 257, count, outputMode, baudrate);
			measure_latency(1024, count, outputMode, baudrate);
		} else {
			while (optind < argc) {
				int blocksize;
				if (argv[optind][0]=='i') {
					blocksize = -1;
				} else {
			       		blocksize = atoi(argv[optind]);
				}
				measure_latency(blocksize, 10, outputMode, baudrate);
				optind++;
			}
		}
	}

	sioTracer->RemoveAllTracers();
	return 0;
}
