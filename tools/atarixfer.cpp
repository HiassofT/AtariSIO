/*
   atarixfer.cpp - transfer disk images from/to an Atari disk drive

   Copyright (C) 2002-2004 Matthias Reichl <hias@horus.com>

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
	unsigned char buf[256];
	int result;


	printf("reading sector 4 ..."); fflush(stdout);
	{
		SIO_parameters params;
		params.device_id = 48+drive_no;
		params.command = 'R';
		params.direction = 0;
		params.timeout = 7;
		params.data_buffer = 0;
		params.data_length = 0;
		params.aux1 = 4;
		params.aux2 = 0;

		result = SIO->DirectSIO(params);
	}
	//result = SIO->ReadSector(drive_no, 4, buf, 0);
	
	if (result) {
		printf(" failed ");
		print_error(result);
	} else {
		printf(" OK\n");
	}

	sleep(1);

	/*
	 * try percom get
	 */

	printf("get percom block ..."); fflush(stdout);
	result = SIO->PercomGet(drive_no, buf);
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
	result = SIO->GetStatus(drive_no, buf);
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
	unsigned char buf[256];
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

				if ( (result = SIO->FormatDisk(drive_no, buf, bytesPerSector)) ) {
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
		result = SIO->FormatDisk(drive_no, buf, 128);
		break;
	case e130kDisk:
		printf("using ED format command\n");
		result = SIO->FormatEnhanced(drive_no, buf);
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

	bool OK = true;
	unsigned char buf[256];

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
		printf("reading sector 721 ..."); fflush(stdout);
		
		// XF551 workaround for QD
		if (SIO->ReadSector(drive_no, 721, buf, 256) == 0) {
			printf(" OK => quad density disk\n");
			total_sectors = 1440;
			image.CreateImage(e360kDisk);
		} else {
			printf(" ERROR => double density disk\n");
			image.CreateImage(e180kDisk);
		}
		image.CreateImage(e360kDisk);
	} else if (total_sectors == 1440 && sector_length == 256) {
		printf("reading sector 721 ..."); fflush(stdout);

		// XF551 workaround for QD
		if (SIO->ReadSector(drive_no, 721, buf, 256) == 0) {
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

		result = SIO->ReadSector(drive_no, sec, buf, length);

		if (result) {
			printf("\nReadSector failed ");
			print_error(result);
			OK = false;
			break;
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

	int result,length;
	unsigned int total_sectors;
	unsigned int sec;
	EDiskFormat diskFormat;

	unsigned char buf[256];

	diskFormat = image.GetDiskFormat();
	total_sectors = image.GetNumberOfSectors();

	if (format_disk(diskFormat)) {
		goto failure;
	}

	printf("writing image to disk:\n");
	for (sec=1; sec <= total_sectors; sec++) {
		printf("\b\b\b\b\b%5d", sec); fflush(stdout);
		length = image.GetSectorLength(sec);
		if (length == 0) {
			printf("error getting sector length\n");
			return 1;
		}

		if (!image.ReadSector(sec, buf, length)) {
			printf("\nerror accessing sector %d of image\n",sec);
			goto failure;
		}

		if ((result = SIO->WriteSector(drive_no, sec, buf, length)) ) {
			printf("\nerror writing sector %d to disk ", sec);
			print_error(result);
			goto failure;
		}
	}
	printf("\nwriting finished successfully!\n");

	return 0;

failure:
	return 1;
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

	char* atarisioDevName = getenv("ATARIXFER_DEVICE");

	printf("atarixfer %s\n(c) 2002-2007 by Matthias Reichl <hias@horus.com>\n\n",VERSION_STRING);
	while(!finished) {
		c = getopt(argc, argv, "prw12345678df:");
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
		default:
			printf("forgot to catch option %d\n",c);
			break;
		}
	}
	if (debugging) {
		sioTracer->SetTraceGroup(SIOTracer::eTraceInfo, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceDebug, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceVerboseCommands, true, tracer);
	}
	if (optind + 1 != argc) {
		goto usage;
	}

	if (mode == 0 || mode ==1) {
		try {
			SIO = new SIOWrapper(atarisioDevName);
		}
		catch (ErrorObject& err) {
			fprintf(stderr, "%s\n", err.AsString());
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
	printf("usage: [-f device ] [-p] [-d] [-12345678] -r|-w imagefile\n\n");
	printf("  -f device     use alternative AtariSIO device (default: /dev/atarisio0)\n");
	printf("  -p            use APE prosystem cable (default: 1050-2-PC cable)\n");
	printf("  -r imagefile  create ATR/XFD/DCM image of disk\n");
	printf("  -w imagefile  write given ATR/XFD/DCM image to disk\n");
	printf("  -d            enable debugging\n");
	printf("  -12345678     use drive number 1..8\n");
	return 1;
}
