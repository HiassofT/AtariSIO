/*
   atarixfer.cpp - transfer disk images from/to an Atari disk drive

   Copyright (C) 2002-2011 Matthias Reichl <hias@horus.com>

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
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include <sched.h>
#include <string.h>
#include <signal.h>

#include "AtrMemoryImage.h"
#include "SIOWrapper.h"
#include "SIOTracer.h"
#include "FileTracer.h"
#include "Error.h"
#include "MiscUtils.h"

#include "Version.h"

static RCPtr<SIOWrapper> SIO;
static SIOTracer* sioTracer = 0;

static unsigned int drive_no = 1;

static int debugging = 0;

static unsigned int num_retries = 0;

static unsigned int highspeed_mode = ATARISIO_EXTSIO_SPEED_NORMAL;

static bool xf551_format_detection = false;

static bool continue_on_errors = false;

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

static void print_error(int error)
{
	if (error > ATARISIO_ERRORBASE) {
		printf("[atari error %d]\n",error - ATARISIO_ERRORBASE);
	} else {
		printf("[system error %d]\n",error);
	}
}

static int get_density(unsigned int& bytesPerSector, unsigned int& sectorsPerDisk)
{
	uint8_t buf[256];
	int result;


	if (xf551_format_detection) {
		printf("reading sector 4 ..."); fflush(stdout);

		result = SIO->ImmediateCommand(drive_no, 'R', 4, 0, 7);
		
		if (result) {
			printf(" failed ");
			print_error(result);
		} else {
			printf(" OK\n");
		}

		sleep(1);
	}

	/*
	 * try percom get
	 */

	printf("get percom block ..."); fflush(stdout);
	result = SIO->PercomGet(drive_no, buf, highspeed_mode);
	if (result) {
		printf(" failed ");
		print_error(result);
	} else {
		printf(" OK\n");

		SIOTracer::GetInstance()->TraceDecodedPercomBlock(drive_no, buf, true, false);
		bytesPerSector = buf[7] + 256*buf[6];
		if (bytesPerSector != 128 && bytesPerSector != 256) {
			return 1;
		}

		int secPerTrack = buf[3] + 256*buf[2];
		int sides = buf[4] + 1;
		int tracks = buf[0];
		
		sectorsPerDisk = secPerTrack * tracks * sides;	

		printf("[%d BytesPerSec, %d Sectors]\n", bytesPerSector, sectorsPerDisk);
		return 0;
	}

	/*
	 * if the drive doesn't support percom get/put, try to figure
	 * out the density by using the status command
	 */

	printf("get status ..."); fflush(stdout);
	result = SIO->GetStatus(drive_no, buf, highspeed_mode);
	if (result) {
		printf(" failed ");
		print_error(result);
		return 1;
	} else {
		printf(" OK\n");
		if (buf[0] & 0x80) {
			/* enhanced density */
			bytesPerSector = 128;
			sectorsPerDisk = 1040;
			return 0;
		}
		if (buf[0] & 0x20) {
			bytesPerSector = 256;
			sectorsPerDisk = 720;
			return 0;
		}
		bytesPerSector = 128;
		sectorsPerDisk = 720;
		return 0;
	}

	return 1;
}

static int format_disk(EDiskFormat diskFormat)
{
	uint8_t buf[256];
	int result;
	int bytesPerSector;

	/*
	 * try to configure disk with percom get & put
	 */

	if (diskFormat != e90kDisk &&
	    diskFormat != e130kDisk &&
	    diskFormat != e180kDisk &&
	    diskFormat != e360kDisk) {
		printf("unsupported disk format!\n");
		return 1;
	}

	if (diskFormat != e130kDisk) {
		printf("reading percom block\n");
		if (!(result = SIO->PercomGet(drive_no, buf)) ) {

			/*
			 * Drive seems to be percom-configurable, now
			 * try to set density for format command
			 */
			switch (diskFormat) {
			case e90kDisk:
				buf[0] = 40;
				buf[2] = 0;
				buf[3] = 18;
				buf[4] = 0;
				buf[5] = 0;
				buf[6] = 0;
				buf[7] = 128;
				bytesPerSector = 128;
				printf("single density\n");
				break;
			/*
			case e130kDisk:
				buf[0] = 40;
				buf[2] = 0;
				buf[3] = 26;
				buf[4] = 0;
				buf[5] = 4;
				buf[6] = 0;
				buf[7] = 128;
				bytesPerSector = 128;
				printf("enhanced density\n");
				break;
			*/
			case e180kDisk:
				buf[0] = 40;
				buf[2] = 0;
				buf[3] = 18;
				buf[4] = 0;
				buf[5] = 4;
				buf[6] = 1;
				buf[7] = 0;
				bytesPerSector = 256;
				printf("double density\n");
				break;
			case e360kDisk:
				buf[0] = 40;
				buf[2] = 0;
				buf[3] = 18;
				buf[4] = 1;
				buf[5] = 4;
				buf[6] = 1;
				buf[7] = 0;
				bytesPerSector = 256;
				printf("quad density\n");
				break;
			default:
				printf("illegal disk format\n");
				return 1;
			}

			printf("writing new percom block\n");
			if ( !(result = SIO->PercomPut(drive_no, buf)) ) {
				printf("disk configured, starting formatting\n");

				if ( (result = SIO->FormatDisk(drive_no, buf, bytesPerSector, highspeed_mode)) ) {
					printf("error formatting disk ");
					print_error(result);
					return 1;
				} else {
					printf("disk formatting OK\n");
					return 0;
				}
			} else {
				printf("failed: ");
				print_error(result);
			}
		} else {
			printf("failed: ");
			print_error(result);
		}
	}
	switch (diskFormat) {
	case e90kDisk:
		printf("using SD format command\n");
		result = SIO->FormatDisk(drive_no, buf, 128, highspeed_mode);
		break;
	case e130kDisk:
		printf("using ED format command\n");
		result = SIO->FormatEnhanced(drive_no, buf, highspeed_mode);
		break;
	case e180kDisk:
		printf("disk is not double-density capable!\n");
		return 1;
	default:
		printf("illegal disk format\n");
		return 1;
	}
	if ( result ) {
		printf("error formatting disk ");
		print_error(result);
		return 1;
	} else {
		printf("disk formatting OK\n");
		return 0;
	}
}

static int read_image(char* filename)
{
	AtrMemoryImage image;

	int result,length;
	unsigned int total_sectors, sector_length;
	unsigned int sec;
	unsigned int retry;

	bool OK = true;
	uint8_t buf[256];

	if (get_density(sector_length, total_sectors)) {
		printf("cannot determine density!\n");
		goto failure;
	}

	if (total_sectors == 720 && sector_length == 128) {
		printf("single density disk\n");
		image.CreateImage(e90kDisk);
	} else if (total_sectors == 1040 && sector_length == 128) {
		printf("enhanced density disk\n");
		image.CreateImage(e130kDisk);
	} else if (total_sectors == 720 && sector_length == 256) {
		if (xf551_format_detection) {
			printf("reading sector 721 ..."); fflush(stdout);
			
			// XF551 workaround for QD
			if (SIO->ReadSector(drive_no, 721, buf, 256, highspeed_mode) == 0) {
				printf(" OK => quad density disk\n");
				total_sectors = 1440;
				image.CreateImage(e360kDisk);
			} else {
				printf(" ERROR => double density disk\n");
				image.CreateImage(e180kDisk);
			}
		} else {
			printf("double density disk\n");
			image.CreateImage(e180kDisk);
		}
	} else if (total_sectors == 1440 && sector_length == 256) {
		printf("reading sector 721 ..."); fflush(stdout);

		// XF551 workaround for QD
		if (SIO->ReadSector(drive_no, 721, buf, 256, highspeed_mode) == 0) {
			printf(" OK => quad density disk\n");
			image.CreateImage(e360kDisk);
		} else {
			total_sectors = 720;
			printf(" ERROR => double density disk\n");
			image.CreateImage(e180kDisk);
		}
	} else {
		if ( (total_sectors < 1) || (total_sectors > 65535) ||
			( (sector_length != 128) && (sector_length != 256) ) ) {
			printf("illegal disk format: %d sectors / %d bytes per sector\n",
				total_sectors, sector_length);
			goto failure;
		}
		switch (sector_length) {
		case 128:
			printf("%d sectors single density disk\n", total_sectors);
			image.CreateImage(e128BytesPerSector, total_sectors);
			break;
		case 256:
			printf("%d sectors double density disk\n", total_sectors);
			image.CreateImage(e256BytesPerSector, total_sectors);
			break;
		}
	}
	
	printf("starting to read disk\n");

	for (sec=1;sec<=total_sectors;sec++) {
		printf("\b\b\b\b\b%5d",sec); fflush(stdout);
	
		length = 128;
		if ( (sector_length == 256) && (sec>3) ) {
			length = 256;
		}

		retry = num_retries+1;
		do {
			result = SIO->ReadSector(drive_no, sec, buf, length, highspeed_mode);
			retry--;
		} while (result && retry);

		if (result) {
			printf("\nerror reading sector %d from disk ", sec);
			print_error(result);
			if (!continue_on_errors) {
				OK = false;
				break;
			}
		}
		if (!image.WriteSector(sec, buf, length)) {
			printf("\nunable to write sector %d to atr image!\n",sec);
			OK = false;
			break;
		}
	}
	if (OK) {
		if (!image.WriteImageToFile(filename)) {
			printf("\nwriting \"%s\" failed!\n", filename);
			OK = false;
		}
	}

	if (OK) {
		printf("\nsuccessfully created \"%s\" from disk\n", filename);
		return 0;
	}
failure:
	return 1;
}

static int write_image(char *filename)
{
	AtrMemoryImage image;

	if (!image.ReadImageFromFile(filename)) {
		printf("error reading \"%s\"\n", filename);
		return 1;
	}

	uint8_t zero[256];

	memset(zero, 0, 256);

	int result, length;
	unsigned int total_sectors;
	unsigned int sec;
	unsigned int retry;
	EDiskFormat diskFormat;

	uint8_t buf[256];

	diskFormat = image.GetDiskFormat();
	total_sectors = image.GetNumberOfSectors();

	if (format_disk(diskFormat)) {
		goto failure;
	}

	printf("writing image to disk:\n");
	for (sec=1; sec <= total_sectors; sec++) {
		length = image.GetSectorLength(sec);
		if (length == 0) {
			printf("error getting sector length\n");
			return 1;
		}
		if (!image.ReadSector(sec, buf, length)) {
			printf("\nerror accessing sector %d of image\n",sec);
			goto failure;
		}
		if (memcmp(buf, zero, length) == 0) {
			continue;
		}

		printf("\b\b\b\b\b%5d", sec); fflush(stdout);

		retry = num_retries+1;
		do {
			result = SIO->WriteSector(drive_no, sec, buf, length, highspeed_mode);
			retry--;
		} while (result && retry);
		
		if (result) {
			printf("\nerror writing sector %d to disk ", sec);
			print_error(result);
			if (!continue_on_errors) {
				goto failure;
			}
		}
	}
	printf("\nwriting finished successfully!\n");

	return 0;

failure:
	return 1;
}

static bool set_and_check_highspeed_baudrate(unsigned int baud)
{
	if (SIO->SetHighSpeedBaudrate(baud) == 0) {
		SIO->SetBaudrate(baud);
		int real_baud = SIO->GetExactBaudrate();
		SIO->SetBaudrate(ATARISIO_STANDARD_BAUDRATE);
		if (real_baud != (int)baud) {
			printf("warning: UART doesn't support %d baud, using %d instead\n", baud, real_baud);
		}
		return true;
	} else {
		printf("switching to %d baud failed\n", baud);
		return false;
	}
}

static bool check_ultraspeed()
{
	uint8_t pokey_div;
	unsigned int baud;

        Ext_SIO_parameters params;

        params.device = 0x31;
	params.unit = drive_no;
        params.command = 0x3f;
        params.direction = ATARISIO_EXTSIO_DIR_RECV;
        params.timeout = 1;
        params.data_buffer = &pokey_div;
        params.data_length = 1;
	params.highspeed_mode = ATARISIO_EXTSIO_SPEED_NORMAL;

	int ret = SIO->ExtSIO(params);

	if (ret == 0) {
		if (!MiscUtils::PokeyDivisorToBaudrate(pokey_div, baud, true)) {
			printf("unsupported ultra speed pokey divisor %d\n", pokey_div);
			return false;
		}
		printf("detected ultra speed drive: pokey divisor %d (%d baud)\n", pokey_div, baud);
		if (pokey_div == 10) {
			printf("possibly Happy 1050: enabling fast writes\n");
			params.command = 0x48;
			params.data_length = 0;
			params.aux1 = 0x20;
			params.aux2 = 0;
			if (SIO->ImmediateCommand(drive_no, 0x48, 0x20, 0) == 0) {
				printf("Happy command $48 (AUX=$20) succeeded\n");
			} else {
				printf("Happy command $48 (AUX=$20) failed\n");
			}
		}
		if (set_and_check_highspeed_baudrate(baud)) {
			highspeed_mode = ATARISIO_EXTSIO_SPEED_ULTRA;
			return true;
		}
	}
	return false;
}

static bool check_happy_1050()
{
	if (SIO->ImmediateCommand(drive_no, 0x48, 0x20, 0, 1) == 0) {
		printf("detected Happy Warp Speed drive\n");
		unsigned int baud;
		if (!MiscUtils::PokeyDivisorToBaudrate(16, baud, true)) {
			printf("internal error: cannot get XF551 baudrate\n");
			return false;
		}
		if (set_and_check_highspeed_baudrate(baud)) {
			highspeed_mode = ATARISIO_EXTSIO_SPEED_WARP;
			return true;
		}
	}
	return false;
}

static bool check_high_status(unsigned int baudrate, uint8_t highspeed_mode)
{
	uint8_t buf[4];
	if (SIO->SetHighSpeedBaudrate(baudrate)) {
		printf("setting baudrate to %d failed\n", baudrate);
		return false;
	}
	return (SIO->GetStatus(drive_no, buf, highspeed_mode) == 0);
}

static bool check_turbo_1050()
{
	unsigned int baud;
	if (!MiscUtils::PokeyDivisorToBaudrate(6, baud, true)) {
		printf("internal error: cannot get 1050 Turbo baudrate\n");
		return false;
	}
	if (check_high_status(baud, ATARISIO_EXTSIO_SPEED_TURBO)) {
		printf("detected 1050 Turbo\n");
		if (set_and_check_highspeed_baudrate(baud)) {
			highspeed_mode = ATARISIO_EXTSIO_SPEED_TURBO;
			return true;
		}
	}
	return false;
}

static bool check_xf551()
{
	unsigned int baud;
	if (!MiscUtils::PokeyDivisorToBaudrate(16, baud, true)) {
		printf("internal error: cannot get XF551 baudrate\n");
		return false;
	}
	if (check_high_status(baud, ATARISIO_EXTSIO_SPEED_XF551)) {
		printf("detected XF551\n");
		if (!xf551_format_detection) {
			xf551_format_detection = true;
			printf("enabled workaround for XF551 format detection bugs\n");
		}
		if (set_and_check_highspeed_baudrate(baud)) {
			highspeed_mode = ATARISIO_EXTSIO_SPEED_XF551;
			return true;
		}
	}
	return false;
}

static bool detect_highspeed_mode(bool include_16c950_modes)
{
	printf("checking highspeed capability\n");
	if (include_16c950_modes) {
		if (check_ultraspeed()) {
			return true;
		}
		if (check_turbo_1050()) {
			return true;
		}
	}
	if (check_happy_1050()) {
		return true;
	}
	if (check_xf551()) {
		return true;
	}
	printf("no highspeed drive detected, using standard speed\n");
	return false;
}

#ifdef ALL_IN_ONE
int atarixfer_main(int argc, char** argv)
#else
int main(int argc, char** argv)
#endif
{
	sioTracer = SIOTracer::GetInstance();
	RCPtr<FileTracer> tracer(new FileTracer(stderr));
	sioTracer->AddTracer(tracer);

	int c;

	int mode=-1; /* 0=read, 1=write */
	int finished = 0;
	int prosys = 0;
	int ret;
	int use_highspeed = 0;
	int relaxed_transmission = 0;

	char* atarisioDevName = getenv("ATARIXFER_DEVICE");

	printf("atarixfer %s\n(c) 2002-2011 by Matthias Reichl <hias@horus.com>\n\n",VERSION_STRING);
	while(!finished) {
		c = getopt(argc, argv, "prw12345678def:R:sStx");
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'r':
			if (mode==-1) {
				mode = 0;
			} else {
				goto usage;
			}
			break;
		case 'w':
			if (mode==-1) {
				mode = 1;
			} else {
				goto usage;
			}
			break;
		case 'p':
			if (prosys == 0) {
				prosys = 1;
			} else {
				goto usage;
			}
			break;
		case 'd':
			debugging=1;
			break;
		case 'e':
			continue_on_errors = true;
			break;
		case 'f':
			atarisioDevName = optarg;
			break;
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
			drive_no=c - '0';
			break;
		case 'R':
			{
				num_retries = strtol(optarg, NULL, 0);
				if (num_retries > 100) {
					printf("invalid number of retries\n");
					goto usage;
				}
			}
		case 's':
			use_highspeed = 1;
			break;
		case 'S':
			use_highspeed = 2;
			break;
		case 't':
			relaxed_transmission = 1;
			break;
		case 'x':
			xf551_format_detection = true;
			break;
		default:
			printf("forgot to catch option %d\n",c);
			break;
		}
	}
	if (debugging) {
		sioTracer->SetTraceGroup(SIOTracer::eTraceInfo, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceWarning, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceError, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceDebug, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceVerboseCommands, true, tracer);
	}
	if (optind + 1 != argc) {
		goto usage;
	}

	if (mode == 0 || mode == 1) {
		try {
			SIO = new SIOWrapper(atarisioDevName);
		}
		catch (ErrorObject& err) {
			std::cerr << err.AsString() << std::endl;
			exit (1);
		}
        	if (MiscUtils:: set_realtime_scheduling(0)) {
/*
#ifdef ATARISIO_DEBUG
			ALOG("the server will automatically terminate in 5 minutes!");
			signal(SIGALRM, my_sig_handler);
			alarm(300);
#endif
*/
		}

		if (prosys) {
			ret= SIO->SetCableType_APE_Prosystem();
		} else {
			ret = SIO->SetCableType_1050_2_PC();
		}

		if (ret) {
			printf("cannot set %s mode\n", prosys ? "prosystem" : "1050-2-PC");
			return 1;
		}

		if (relaxed_transmission) {
			ret = SIO->SetHighSpeedPause(ATARISIO_HIGHSPEEDPAUSE_BYTE_DELAY);
			if (ret) {
				printf("couldn't set relaxed data transmission: error %d\n", ret);
			}
		}

		if (use_highspeed) {
			detect_highspeed_mode(use_highspeed == 2);
		}
		if (mode == 0) {
			ret = read_image(argv[optind]);
		} else {
			ret = write_image(argv[optind]);
		}
		sioTracer->RemoveAllTracers();
		return ret;
	} else {
		goto usage;
	}
usage:
	printf("usage: [-f device ] [options] -r|-w imagefile\n\n");
        printf("options:\n");
	printf("  -f device     use alternative AtariSIO device (default: /dev/atarisio0)\n");
	printf("  -r imagefile  create ATR/XFD/DCM image of disk\n");
	printf("  -w imagefile  write given ATR/XFD/DCM image to disk\n");
	printf("  -d            enable debugging\n");
	printf("  -e            continue on errors\n");
	printf("  -p            use APE prosystem cable (default: 1050-2-PC cable)\n");
	printf("  -R num        retry failed sector I/O 'num' times (0..100)\n");
	printf("  -s            enable Happy Warp/XF551 speeds\n");
	printf("  -S            enable Ultra/Turbo/Happy Warp/XF551 speeds\n");
	printf("  -t            enable relaxed data transmission\n");
	printf("  -x            enable workaround for XF551 format detection bugs\n");
	printf("  -1 ... -8     use drive number 1...8\n");
	return 1;
}
