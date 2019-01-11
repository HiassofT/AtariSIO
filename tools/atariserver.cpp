/*
   atariserver.cpp - implementation of an Atari SIO server, using
   a curses frontend

   Copyright (C) 2003-2019 Matthias Reichl <hias@horus.com>

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

#include "CursesFrontend.h"
#include "CursesFrontendTracer.h"
#include "SIOTracer.h"
#include "FileTracer.h"
#include "History.h"
#include "Error.h"
#include "AtariDebug.h"
#include "MiscUtils.h"
#include "Version.h"
#include "RemoteControlHandler.h"

#include <iostream>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

static CursesFrontend* frontend= NULL;

static SIOTracer* sioTracer = 0;

static void my_sig_handler(int sig)
{
	ALOG("got signal %d", sig);
	switch (sig) {
	case SIGWINCH:
		if (frontend) {
			frontend->IndicateGotSigWinCh();
			//ungetch(KEY_RESIZE);
		}
		break;
	case SIGALRM:
	case SIGINT:
	case SIGTERM:
	case SIGHUP:
		if (sioTracer) {
			sioTracer->RemoveAllTracers();
		}
		endwin();
		exit(0);
	case SIGPIPE:
	case SIGCHLD:
		break;
	}
}

static int trace_level = 0;

static const char* cas_filename = 0;

static void process_args(RCPtr<DeviceManager>& manager, CursesFrontend* frontend, int argc, char** argv)
{
	bool write_protect_next = false;
	EDiskFormat virtual_format = e130kDisk;
	ESectorLength virtual_sector_length = e128BytesPerSector;
	int virtual_sector_count = 1040;

	int drive = 1;
	bool autoSectors;

	for (int i=1;i<argc;i++) {
		if (argv[i]) {
			int len=strlen(argv[i]);
			if (len == 0) {
				endwin();
				printf("illegal argv!\n");
				exit(1);
			}
			if (argv[i][0]=='-') {
				switch(argv[i][1]) {
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
					if (len != 2) {
						goto illegal_option;
					}
					drive = argv[i][1] - '0';
					break;
				case 't':
					if (len != 2) {
						goto illegal_option;
					}
					trace_level++;
					break;
				case 'B':
					if (i + 1 < argc) {
						i++;
						unsigned int p = atoi(argv[i]);
						manager->SetTapeSpeedPercent(p);
					} else {
						AERROR("-B needs a parameter!");
					}
					break;
				case 's':
					if (i + 1 < argc) {
						i++;
						switch (argv[i][0]) {
						case '0':
							manager->SetHighSpeedMode(false);
							ALOG("disabling high-speed SIO");
							break;
						case '1':
							manager->SetHighSpeedMode(true);
							ALOG("enabling high-speed SIO");
							break;
						default:
							AERROR("illegal parameter for -s: must be 0 or 1");
							break;
						}
					} else {
						AERROR("-s needs a parameter!");
					}
					break;
				case 'S':
					if (i + 1 < argc) {
						i++;
						unsigned int baud;
						uint8_t divisor;
						if (MiscUtils::ParseHighSpeedParameters(argv[i], divisor, baud)) {
							if (manager->SetHighSpeedParameters(divisor, baud)) {
								ALOG("Configured high speed mode to pokey divisor %d / %d baud", divisor, manager->GetHighSpeedBaudrate());
							} else {
								AERROR("setting high speed parameters (divisor %d, baud %d) failed!", divisor, baud);
							}
						} else {
							AERROR("invalid highspeed parameters in -S: use speed,divisor");
						}
					} else {
						AERROR("-S needs a parameter!");
					}
					break;
				case 'T':
					if (i + 1 < argc) {
						i++;
						switch (argv[i][0]) {
						case 's':
							manager->SetSioTiming(SIOWrapper::eStrictTiming);
							ALOG("using strict SIO timing");
							break;
						case 'r':
							manager->SetSioTiming(SIOWrapper::eRelaxedTiming);
							ALOG("using relaxed SIO timing");
							break;
						default:
							AERROR("invalid SIO timing mode: must be s or r");
							break;
						}
					} else {
						AERROR("-T needs a parameter!");
					}
					break;
				case 'X':
					manager->EnableXF551Mode(true);
					ALOG("enabling XF551 commands");
					break;
				case 'C':
					manager->SetSioServerMode(SIOWrapper::eCommandLine_CTS);
					ALOG("using alternative SIO2PC cable type (command=CTS)");
					break;
				case 'D':
					manager->SetSioServerMode(SIOWrapper::eCommandLine_DSR);
					ALOG("using alternative SIO2PC cable type (command=DSR)");
					break;
				case 'N':
					manager->SetSioServerMode(SIOWrapper::eCommandLine_None);
					ALOG("using SIO2PC cable without command line");
					break;
				case 'F':
					manager->EnableStrictFormatChecking(true);
					ALOG("disable non-standard disk formats\n");
					break;
				case 'p':
					write_protect_next = true;
					break;
				case 'P':
					i++;
					if (i + 1 < argc) {
						PrinterHandler::EEOLConversion conv;
						switch (argv[i][0]) {
						case 'r':
						case 'R':
							conv = PrinterHandler::eRaw; break;
						case 'l':
						case 'L':
							conv = PrinterHandler::eLF; break;
						case 'c':
						case 'C':
							conv = PrinterHandler::eCRLF; break;
						default:
							i++;
							goto illegal_p;
						}
						i++;
						if (!manager->InstallPrinterHandler(argv[i], conv)) {
							AERROR("installing printer handler failed");
						}
					} else {
illegal_p:
						AERROR("invalid usage of -P");
					}
					break;
				case 'V':
					i++;
					if (i + 1 < argc) {
						autoSectors = false;
						switch (argv[i][0]) {
						case 's':
							virtual_format = e90kDisk;
							break;
						case 'e':
							virtual_format = e130kDisk;
							break;
						case 'd':
							virtual_format = e180kDisk;
							break;
						default:
							if (argv[i][0] == 'S') {
								virtual_sector_length = e128BytesPerSector;
								virtual_format = eUserDefDisk;
								autoSectors = true;
							} else if (argv[i][0] == 'D') {
								virtual_sector_length = e256BytesPerSector;
								virtual_format = eUserDefDisk;
								autoSectors = true;
							} else {
								if (argv[i][0] >= '0' && argv[i][0]<='9') {
									int l = strlen(argv[i]);
									if (argv[i][l-1] == 'd' || argv[i][l-1] == 'D') {
										virtual_sector_length = e256BytesPerSector;
									} else if (argv[i][l-1] == 's' || argv[i][l-1] == 's') {
										virtual_sector_length = e128BytesPerSector;
									} else {
										if (argv[i][l-1] < '0' || argv[i][l-1] > '9') {
											i++;
											goto illegal_v;
										}
									}
									virtual_sector_count = atoi(argv[i]);
									if (virtual_sector_count < 720 || virtual_sector_count > 65535) {
										i++;
										goto illegal_v;
									}
									virtual_format = eUserDefDisk;
								}
							}
						}
						i++;
						if (manager->DriveNumberOK(DeviceManager::EDriveNumber(drive))) {
							DeviceManager::EDriveNumber driveNo = DeviceManager::EDriveNumber(drive);
							if (manager->DriveInUse(driveNo)) {
								AERROR("drive D%d: already assigned!", drive);
							}
							bool ok;
							if (virtual_format == eUserDefDisk) {
								if (autoSectors) {
									virtual_sector_count = Dos2xUtils::EstimateDiskSize(argv[i], virtual_sector_length, Dos2xUtils::ePicoName);
									ALOG("using %d sectors for virtual drive D%d:", virtual_sector_count, drive);
								}
								ok = manager->CreateVirtualDrive(driveNo, argv[i],
									virtual_sector_length, virtual_sector_count);
							} else {
								ok = manager->CreateVirtualDrive(driveNo, argv[i], virtual_format);
							}
							if (!ok) {
								AERROR("creating virtual drive D%d: from \"%s\" failed",
									driveNo, argv[i]);
							} else {
								if (write_protect_next) {
									if (!manager->SetWriteProtectImage(driveNo, true)) {
										AERROR("write protecting D%d: failed!", drive);
									}
									write_protect_next = false;
								}
								drive++;
							}
						} else {
							AERROR("too many images - there is no drive D%d:",drive);
						}

					} else {
illegal_v:
						AERROR("invalid usage of -V");
					}
					break;
				case 'Q':
					frontend->SetAskBeforeQuit(true);
					break;
				default:
illegal_option:
					AERROR("illegal option \"%s\"", argv[i]);
				}
			} else {
				int len = strlen(argv[i]);
				if ( ( (len >= 4) && (strcasecmp(argv[i]+len-4, ".cas") == 0) ) ||
				     ( (len >= 7) && (strcasecmp(argv[i]+len-7, ".cas.gz") == 0) ) ) {
					if (cas_filename) {
						AERROR("only one CAS image allowed");
					} else {
						cas_filename = argv[i];
					}
				} else {
					if (manager->DriveNumberOK(DeviceManager::EDriveNumber(drive))) {
						DeviceManager::EDriveNumber driveNo = DeviceManager::EDriveNumber(drive);
						if (manager->DriveInUse(driveNo)) {
							AERROR("drive D%d: already assigned!", drive);
						}
						if (!manager->LoadDiskImage(driveNo, argv[i])) {
							AERROR("cannot load \"%s\" into D%d:", argv[i], drive);
						} else {
							if (write_protect_next) {
								if (!manager->SetWriteProtectImage(driveNo, true)) {
									AERROR("write protecting D%d: failed!", drive);
								}
								write_protect_next = false;
							}
							drive++;
						}
						frontend->AddFilenameHistory(manager->GetImageFilename(driveNo));
					} else {
						AERROR("too many images - there is no drive D%d:",drive);
					}
				}
			}
		}
	}
}

static void SetDefaultTraceLevels(const RCPtr<AbstractTracer>& tracer)
{
	sioTracer->SetTraceGroup(SIOTracer::eTraceInfo, true, tracer);
	sioTracer->SetTraceGroup(SIOTracer::eTraceWarning, true, tracer);
	sioTracer->SetTraceGroup(SIOTracer::eTraceError, true, tracer);
}

void usage()
{
	printf("atariserver %s\n", VERSION_STRING);
	printf("(c) 2002-2019 Matthias Reichl <hias@horus.com>\n");
	printf("usage: [-f device] [options...]\n");
	printf("-h            display help\n");
	printf("-f device     use alternative AtariSIO device (default: /dev/atarisio0)\n");
	printf("-C            use SIO2PC cable with command line on CTS\n");
	printf("-D            use SIO2PC cable with command line on DSR\n");
	printf("-N            use SIO2PC cable without command line\n");
	printf("-F            disable non-standard disk formats\n");
	printf("-m            monochrome mode\n");
	printf("-o file       save trace output to <file>\n");
	printf("-s mode       high speed mode: 0 = off, 1 = on (default)\n");
	printf("-S div[,baud] high speed SIO pokey divisor (default 8) and optionally baudrate\n");
	printf("-T timing     SIO timing: s = strict, r = relaxed\n");
	printf("-X            enable XF551 commands\n");
	printf("-t            increase SIO trace level (default:0, max:3)\n");
	printf("-B percent    set tape baudrate to x%% of nominal speed (1-200)\n");
	printf("-P mode file  install printer handler\n");
	printf("              mode sets EOL conversion: r=raw/none, l=LF, c=CR, b=CR+LF\n");
	printf("              path is either a filename or |print-command, eg |lpr\n");
	printf("-Q            ask before quitting atariserver\n");
	printf("-p            write protect the next image\n");
	printf("-1..-8        set drive number for next image / virtual drive\n"); 
	printf("-V dens dir   create virtual drive of given density, the second parameter\n");
	printf("              specifies the directory. dens is s|e|d|(<number>s|d)|S|D\n");
	printf("<filename>    load <filename> into current drive number, and then\n");
	printf("              increment drive number by one\n");
	printf("              if the filename ends with '.cas' or '.cas.gz', the\n");
	printf("              CAS-image is loaded and tape emulation is started\n");
}

#ifdef ALL_IN_ONE
int atariserver_main(int argc, char** argv)
#else
int main(int argc, char** argv)
#endif
{
	RCPtr<DeviceManager> manager;
	char* atarisioDevName = getenv("ATARISERVER_DEVICE");
	if (argc > 1 && (!strcmp(argv[1],"-h") || !strcmp(argv[1],"--help") ) ) {
		usage();
		return 0;
	}
	if (argc > 2 && (strcmp(argv[1],"-f") == 0)) {
		atarisioDevName = argv[2];
		argv[1] = 0;
		argv[2] = 0;
	}
	try {
		manager = new DeviceManager(atarisioDevName);
	}
	catch (ErrorObject& err) {
		std::cerr << err.AsString() << std::endl;
		exit(1);
	}

	if (!MiscUtils::drop_root_privileges()) {
		fprintf(stderr, "error dropping root privileges\n");
		exit(1);
	}

	bool wantHelp = false;
	bool useColor = true;
	const char* traceFile = 0;
	struct sigaction sigact;

	// scan argv for "-h", "-m", -"o file"
	{
		for (int i=1; i<argc; i++) {
			if ( argv[i] && (argv[i][0] == '-') && (argv[i][1] != 0) && (argv[i][2] == 0) ) {
				switch (argv[i][1]) {
				case 'h':
					wantHelp = true;
					argv[i] = 0;
					break;
				case 'm':
					useColor = false;
					argv[i] = 0;
					break;
				case 'o':
					if (i+1 < argc) {
						if (traceFile) {
							printf("only one trace file allowed - ignoring -f %s\n",
								argv[i+1]);
						} else {
							traceFile = argv[i+1];
						}
						argv[i] = 0;
						argv[i+1] = 0;
						i++;
					}
					break;
				default:
					break;
				}
			}
		}
	}

	if (wantHelp) {
		usage();
		return 0;
	}

	//signal(SIGWINCH, my_sig_handler);
	sigact.sa_handler = my_sig_handler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;

	sigaction(SIGWINCH, &sigact, NULL);

	frontend = new CursesFrontend(manager, useColor);

	sioTracer = SIOTracer::GetInstance();
	{
		RCPtr<CursesFrontendTracer> cursesTracer(new CursesFrontendTracer(frontend));
		sioTracer->AddTracer(cursesTracer);
		SetDefaultTraceLevels(cursesTracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceImageStatus, true, cursesTracer);

		if (traceFile) {
			RCPtr<FileTracer> tracer;
			try {
				tracer = new FileTracer(traceFile);
				sioTracer->AddTracer(tracer);
				SetDefaultTraceLevels(tracer);
				sioTracer->SetTraceGroup(SIOTracer::eTraceDebug, true, tracer);
			}
			catch (ErrorObject& err) {
				AERROR("%s", err.AsCString());
			}
		}
	}
	frontend->ShowCursor(false);

/*
	signal(SIGTERM, my_sig_handler);
	signal(SIGINT, my_sig_handler);
	signal(SIGHUP, my_sig_handler);
	signal(SIGPIPE, my_sig_handler);
	signal(SIGCHLD, my_sig_handler);
*/
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGHUP, &sigact, NULL);
	sigaction(SIGPIPE, &sigact, NULL);
	sigaction(SIGCHLD, &sigact, NULL);

	if (MiscUtils:: set_realtime_scheduling(0)) {
#if 0
#ifdef ATARISIO_DEBUG
		ALOG("the server will automatically terminate in 5 minutes!");
		//signal(SIGALRM, my_sig_handler);
		sigaction(SIGALRM, &sigact, NULL);
		alarm(300);
		//alarm(10);
#endif
#endif
	}

	process_args(manager, frontend, argc, argv);

	RCPtr<RemoteControlHandler> remoteControl = new RemoteControlHandler(manager.GetRealPointer(), frontend);
	if (!manager->GetSIOManager()->RegisterHandler(DeviceManager::eSIORemoteControl, remoteControl)) {
		DPRINTF("registering remote control handler failed");
	}

	frontend->DisplayDriveStatus();
	frontend->DisplayPrinterStatus();
	frontend->UpdateScreen();

	frontend->SetTraceLevel(trace_level);
	frontend->DisplayStatusLine();
	frontend->UpdateScreen();

	if (cas_filename) {
		frontend->ProcessTapeEmulation(cas_filename);
	}
	bool running = true;
	do {
		int ch = frontend->GetCh(false);
		switch (ch) {
		case 'a':
			frontend->ProcessActivateDrive();
			break;
		case 'A':
			frontend->ProcessDeactivateDrive();
			break;
		case 'c':
			frontend->ProcessCreateDrive();
			break;
		case 'C':
			frontend->ProcessSetCommandLine();
			break;
		case 'd':
			frontend->ProcessShowDirectory();
			break;
		case 'f':
			frontend->ProcessFormatDrive();
			break;
		case 'F':
			frontend->ProcessFlushPrinterData();
			break;
		case 'i':
			frontend->ProcessInstallPrinterHandler();
			break;
		case 'I':
			frontend->ProcessUninstallPrinterHandler();
			break;
		case 'h':
			frontend->ProcessShowHelp();
			break;
		case 'l':
			frontend->ProcessLoadDrive();
			break;
		case 'p':
			frontend->ProcessWriteProtectDrive();
			break;
		case 'P':
			frontend->ProcessUnprotectDrive();
			break;
		case 'q':
			if (frontend->ProcessQuit()) {
				running = false;
			}
			break;
		case 'r':
			frontend->ProcessReloadDrive();
			break;
		case 's':
			frontend->ProcessSetHighSpeed();
			break;
		case 'S':
			frontend->ProcessSetHighSpeedParameters();
			break;
		case 'T':
			frontend->ProcessSetSioTiming();
			break;
		case 't':
			frontend->ProcessSetTraceLevel();
			break;
		case 'u':
			frontend->ProcessUnloadDrive();
			break;
		case 'w':
			frontend->ProcessWriteDrive();
			break;
		case 'x':
			frontend->ProcessExchangeDrives();
			break;
		case 'X':
			frontend->ProcessSetXF551Mode();
			break;
		case 11:
			manager->GetSIOWrapper()->DebugKernelStatus();
			break;
		case 12:
			endwin();
			frontend->InitWindows();
			break;
		case KEY_RESIZE:
			frontend->HandleResize();
			break;
		default:
			beep();
			//DPRINTF("got character 0x%04x ('%c')", ch, ( (ch>=32) && (ch<=126) ) ? ch : '.');
			break;
		}
		if (frontend->GotSigWinCh()) {
			frontend->HandleResize();
		}

	} while (running);

	sioTracer->RemoveAllTracers();
	{
		CursesFrontend* fe = frontend;
		frontend = NULL;
		delete fe;
	}
	return 0;
}
