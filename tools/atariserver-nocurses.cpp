/*
   atariserver-nocurses.cpp - implementation of an Atari SIO server,
   using a simple text-only frontend

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
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include <signal.h>
#include <sched.h>
#include <sys/mman.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <ctype.h>

#include "OS.h"
#include "AtrSIOHandler.h"
#include "AtrMemoryImage.h"
#include "SIOManager.h"
#include "DeviceManager.h"
#include "SIOTracer.h"
#include "FileTracer.h"
#include "Dos2xUtils.h"
#include "Error.h"
#include "AtariDebug.h"

#include "Version.h"

SIOTracer* sioTracer = 0;

static struct termios orig_stdin_tio, noncanon_stdin_tio;

/* set stdin to non-canonical input mode */
static int init_noncanon_stdin_tio()
{
	if (tcgetattr(STDIN_FILENO, &orig_stdin_tio) < 0) {
		DPRINTF("tcgetattr error");
		return 1;
	}
	if (tcgetattr(STDIN_FILENO, &noncanon_stdin_tio) < 0) {
		DPRINTF("tcgetattr error");
		return 1;
	}
	noncanon_stdin_tio.c_lflag &= ~(ICANON | ECHO);

	return 0;
}

static int set_noncanon_stdin_mode()
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &noncanon_stdin_tio) < 0) {
		DPRINTF("tcsetattr error");
		return 1;
	}
	return 0;
}

static int restore_stdin_mode()
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_stdin_tio) < 0) {
		DPRINTF("tcsetattr error");
		return 1;
	}
	return 0;
}

void my_sig_handler(int sig)
{
	switch (sig) {
	case SIGALRM:
		AWARN("got SIGALARM");
		goto quit;
	case SIGINT:
		goto quit;
	quit:
		ALOG("good bye!");
		if (sioTracer) {
			sioTracer->RemoveAllTracers();
		}
		restore_stdin_mode();
		exit(1);
	default:
		break;
	}
}

// use some space on the stack, so that later we won't run into
// problems with stack space (as a side effect of mlockall)

static void reserve_stack_memory()
{
#define RESERVE_STACK_SIZE 100000
	char dummy_string[RESERVE_STACK_SIZE];
	int i;
	for (i=0;i<RESERVE_STACK_SIZE;i++) {
		dummy_string[i]=0;
	}
}

static void set_realtime_scheduling(int priority)
{
	struct sched_param sp;
	pid_t myPid;

	memset(&sp, 0, sizeof(struct sched_param));
	sp.sched_priority = sched_get_priority_max(SCHED_RR) - priority;
	myPid = getpid();

	if (sched_setscheduler(myPid, SCHED_RR, &sp) == 0) {
		ALOG("activated realtime scheduling");

/*
#ifdef ATARISIO_DEBUG
		AWARN("the server will automatically terminate in 5 minutes!");
		signal(SIGALRM, my_sig_handler);
		alarm(300);
#endif
*/
	} else {
		AWARN("cannot set realtime scheduling! please run as root!");
	}

	reserve_stack_memory();
	if (mlockall(MCL_CURRENT) == 0) {
		ALOG("mlockall succeeded");
	} else {
		AWARN("mlockall failed\n");
	}

	// drop root privileges - set eid to real id
	seteuid(getuid());
	setegid(getgid());
}

static int trace_level = 0;
static bool auto_status_update = true;

static void print_status(const RCPtr<DeviceManager>& manager)
{
	printf("+----------------------+------------------------------------------------------+\n");
	printf("|  atariserver status  | ");

     	printf("SIO mode = ");
	switch(manager->GetHighSpeedMode()) {
	case DeviceManager::eHighSpeedOff:
		printf("slow      "); break;
	case DeviceManager::eHighSpeedOn:
		printf("high      "); break;
	case DeviceManager::eHighSpeedWithPause:
		printf("high/pause"); break;
	}

	printf("  auto update = ");
	if (auto_status_update) {
		printf("on ");
	} else {
		printf("off");
	}

	printf("  trace = %d  |", trace_level);

	printf("\n");

	printf("+-----+-------+--------+------------------------------------------------------+\n");
	printf("| DRV | C P D |  size  | filename                                             |\n");
	printf("+-----+-------+--------+------------------------------------------------------+\n");
	int i;
	int maxlen=53;
	for (i=DeviceManager::eMinDriveNumber;i<=DeviceManager::eMaxDriveNumber;i++) {
		printf("| D%d: | ",i);
		if (manager->DriveInUse(DeviceManager::EDriveNumber(i))) {
			if (manager->DriveIsChanged(DeviceManager::EDriveNumber(i))) {
				printf("C ");
			} else {
				printf("  ");
			}

			if (manager->DriveIsWriteProtected(DeviceManager::EDriveNumber(i))) {
				printf("P ");
			} else {
				printf("  ");
			}
			RCPtr<const DiskImage> diskImage = manager->GetConstDiskImage(DeviceManager::EDriveNumber(i));
			if (diskImage) {
				switch (diskImage->GetSectorLength()) {
				case e128BytesPerSector: printf("S | "); break;
				case e256BytesPerSector: printf("D | "); break;
				default: printf("? | "); break;
				}
				printf("%5dk | ", (int) (diskImage->GetImageSize()+512) / 1024);
			} else {
					printf("? |     ?k | ");
			}

			const char * f = manager->GetImageFilename(DeviceManager::EDriveNumber(i));
			if (f) {
				int len=strlen(f);
				if (len<=maxlen) {
					printf("%s", f);
					while (len++<maxlen) {
						printf(" ");
					}
					printf("|");
				} else {
					/* try to shorten the filename */
					const char *p=f+len-maxlen+3;
					len=strlen(p)+3;
					while (*p && *p!= DIR_SEPARATOR) {
						p++; len--;
					}
					if (*p) {
						printf("...%s",p);
						while (len++<maxlen) {
							printf(" ");
						}
						printf("|");
					} else {
						printf("%s",f);
					}
				}
			} else {
				printf("                                                     |");
			}
		} else {
			printf("- - - | ------ | <empty>                                              |");
		}
		printf("\n");
	}
	printf("+-----+-------+--------+------------------------------------------------------+\n");
}

static void print_directory(RCPtr<AtrImage>& image)
{
	Dos2xUtils utils(image);
	RCPtr<Dos2xUtils::Dos2Dir> dir = utils.GetDos2Directory();
        if (dir.IsNull()) {
		AERROR("error reading directory\n");
		return;
	}

	unsigned int columns = 4;
	unsigned int i;

	for (i=0;i<dir->GetNumberOfFiles();i++) {
		printf("%s", dir->GetFile(i));
		if ( i % columns == (columns - 1)) {
			printf("\n");
		}       
	}       
	if (i % columns != 0) {
		printf("\n");
	}
	printf("%d free sectors\n", dir->GetFreeSectors());
}

static void print_auto_status(const RCPtr<DeviceManager>& manager)
{
	if (auto_status_update) {
		print_status(manager);
	}
}

/* menu helper routines */

static int input_char(const char* allowed)
{
	while (1) {
		int c = getchar();
		if (c==EOF || c==27 || c=='\a') {
			return -1;
		}
		if (strchr(allowed,c)) {
			return c;
		}
	}
}

static void print_aborted()
{
	printf("[aborted]\n");
}

static void print_ok()
{
	printf(" - OK\n");
}

static void print_error()
{
	printf(" - ERROR!\n");
}

static DeviceManager::EDriveNumber input_drive_number(const char* all_prompt)
{
	static char * driveChars = 0;
	static char * driveCharsPlusAll = 0;

	if (driveChars == 0) {
		int len = DeviceManager::eMaxDriveNumber-DeviceManager::eMinDriveNumber+2;
		driveChars = (char*) malloc(len);
		driveChars[len-1] = 0;
		for (int i=DeviceManager::eMinDriveNumber;i<=DeviceManager::eMaxDriveNumber;i++) {
			driveChars[i-DeviceManager::eMinDriveNumber] = '0'+i;
		};
		driveCharsPlusAll = (char*) malloc(len+2);
		strcpy(driveCharsPlusAll, driveChars);
		strcat(driveCharsPlusAll,"aA");
	}
	int chr;
	if (all_prompt) {
		chr = input_char(driveCharsPlusAll);
		if (chr=='a' || chr=='A') {
			printf("A"); fflush(stdout);
			return DeviceManager::eAllDrives;
		}
	} else {
		chr = input_char(driveChars);
	}
       
	if (chr > 0) {
		printf("%c", chr);
		fflush(stdout);
		return DeviceManager::EDriveNumber(chr - '0');
	} else {
		print_aborted();
		return DeviceManager::eNoDrive;
	}
}

static void print_drive_number_prompt(const char* prompt, const char* custom)
{
	printf("%s [ %d-%d", prompt,DeviceManager::eMinDriveNumber, DeviceManager::eMaxDriveNumber);
	if (custom) {
		printf(" %s",custom);
	}
	printf(" ESC ] > ");
	fflush(stdout);
}

static DeviceManager::EDriveNumber input_used_drive_number(const RCPtr<DeviceManager>& manager, const char *prompt, const char* all_prompt)
{
	print_drive_number_prompt(prompt, all_prompt);

	DeviceManager::EDriveNumber d = input_drive_number(all_prompt);

	if (all_prompt && d==DeviceManager::eAllDrives) {
		return d;
	}

	if (manager->DriveNumberOK(d) && !manager->DriveInUse(d)) {
		printf(" - no disk in drive D%d:!\n",d);
		return DeviceManager::eNoDrive;
	} else {
		return d;
	}
}

static DeviceManager::EDriveNumber input_unused_drive_number(const RCPtr<DeviceManager>& manager, const char *prompt, const char* all_prompt)
{
	print_drive_number_prompt(prompt, all_prompt);

	DeviceManager::EDriveNumber d = input_drive_number(all_prompt);

	if (all_prompt && d==DeviceManager::eAllDrives) {
		return d;
	}

	if (manager->DriveNumberOK(d) && manager->DriveInUse(d)) {
		printf(" - there's a disk in drive D%d:!\n",d);
		return DeviceManager::eNoDrive;
	} else {
		return d;
	}
}

static bool input_yes_no(const char *prompt)
{
	printf("%s [Y/N] > ", prompt);
	fflush(stdout);
	int i = input_char("yYnN");
	if (i<0) {
		goto no;
	}
	if (toupper(i)=='Y') {
		printf("Y\n");
		return true;
	} else {
no:
		printf("N\n");
		return false;
	}
}

static char * readline_default_text = 0;

static int readline_startup_hook()
{
	if (readline_default_text) {
		rl_insert_text (readline_default_text);
		free(readline_default_text);
		readline_default_text = 0;
		//rl_startup_hook = (rl_hook_func_t *)NULL;
		rl_startup_hook = (Function *)NULL;
	}
	return 0;
}

static void set_readline_default_text(const char* text)
{
	if (readline_default_text) {
		free(readline_default_text);
		readline_default_text=0;
	}
	if (text && *text) {
		readline_default_text = strdup(text);
		rl_startup_hook = readline_startup_hook;
	}
}

static char * my_readline(const char* prompt)
{
	restore_stdin_mode();

	const char * break_tmp = rl_basic_word_break_characters;
	int tmp_app = rl_completion_append_character;
	rl_basic_word_break_characters = "\n";
	rl_completion_append_character = 0;

 	char * filename = readline(prompt);

	rl_basic_word_break_characters = break_tmp;
	rl_completion_append_character = tmp_app;

	set_noncanon_stdin_mode();

	int l = strlen(filename);
	while (l>0 && isspace(filename[l-1])) {
			filename[l---1] = 0;
	}
	return filename;
}

static void set_trace(int level) {
	SIOTracer* sioTracer = SIOTracer::GetInstance();
	sioTracer->SetTraceGroup(SIOTracer::eTraceCommands, level >= 1 );
	sioTracer->SetTraceGroup(SIOTracer::eTraceUnhandeledCommands, level >= 2 );
	sioTracer->SetTraceGroup(SIOTracer::eTraceVerboseCommands, level >= 2 );
	sioTracer->SetTraceGroup(SIOTracer::eTraceAtpInfo, level >= 2 );
	sioTracer->SetTraceGroup(SIOTracer::eTraceDataBlocks, level >= 3 );
}

int main(int argc, char**argv)
{
	sioTracer = SIOTracer::GetInstance();
        {
		RCPtr<FileTracer> tracer(new FileTracer(stderr));
		sioTracer->AddTracer(tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceInfo, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceWarning, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceError, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceDebug, true, tracer);
	}

	RCPtr<DeviceManager> manager;
	try {
       		manager= new DeviceManager;
	}
	catch (ErrorObject& err) {
		AERROR("%s", err.AsCString());
		sioTracer->RemoveAllTracers();
		exit(1);
	}

	int i, drive, ret;

	bool write_protect_next = false;

	printf("atariserver %s\n(c) 2002, 2003 by Matthias Reichl <hias@horus.com>\n\n",VERSION_STRING);

	set_realtime_scheduling(0); // also drops root privileges

	drive = 1; // default: drive D1:

	for (i=1;i<argc;i++) {
		int len=strlen(argv[i]);
		if (len == 0) {
			AERROR("illegal argv!\n");
			sioTracer->RemoveAllTracers();
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
			case 's':
				manager->SetHighSpeedMode(DeviceManager::eHighSpeedOff);
				printf("disabling high-speed SIO\n");
				break;
			case 'S':
				manager->SetHighSpeedMode(DeviceManager::eHighSpeedWithPause);
				printf("enabling high-speed SIO mode with pauses\n");
				break;
			case 'c':
				manager->SetSioServerMode(SIOWrapper::eCommandLine_DSR);
				printf("using alternative SIO2PC cable type (command=DSR)\n");
				break;
			case 'C':
				manager->SetSioServerMode(SIOWrapper::eCommandLine_CTS);
				printf("using alternative SIO2PC/nullmodem cable type (command=CTS)\n");
				break;
			case 'N':
				manager->SetSioServerMode(SIOWrapper::eCommandLine_None);
				ALOG("using SIO2PC cable without command line");
				break;
			case 'a':
				auto_status_update = false;
				break;
			case 'p':
				write_protect_next = true;
				break;
			case 'h':
				goto usage;
			default:
illegal_option:
				printf("illegal option \"%s\"\n", argv[i]);
				goto usage;
			}
		} else {
			if (manager->DriveNumberOK(DeviceManager::EDriveNumber(drive))) {
				DeviceManager::EDriveNumber driveNo = DeviceManager::EDriveNumber(drive);
				if (manager->DriveInUse(driveNo)) {
					printf("drive D%d: already assigned!\n", drive);
					goto usage;
				}
				printf("loading \"%s\" into D%d:\n", argv[i], drive);
				if (!manager->LoadDiskImage(driveNo, argv[i])) {
					printf("cannot load \"%s\"\n", argv[i]);
				} else {
					if (write_protect_next) {
						if (manager->SetWriteProtectImage(driveNo, true)) {
							printf("warning: write protecting D%d: failed!\n", drive);
						}
						write_protect_next = false;
					}

					add_history(argv[i]);
					drive++;
				}
			} else {
				printf("too many images - there is no drive D%d:\n",drive);
				goto usage;
			}
		}
	}

	if (init_noncanon_stdin_tio()) {
		AERROR("cannot init non-canonical stdin mode\n");
		sioTracer->RemoveAllTracers();
		return 1;
	}
	signal(SIGINT, my_sig_handler);
	if (set_noncanon_stdin_mode()) {
		AERROR("cannot set stdin to non-canonical mode\n");
	}

	set_trace(trace_level);

	printf("atariserver is up and running...\n\n");
	print_auto_status(manager);
	printf("\npress 'h' for help\n");

	fflush(stdout);

	{
		bool running = true;
	
		while (running) {
			ret = manager->DoServing(fileno(stdin));
			if (ret==0) {
				uint8_t c;
				int bytes = read(fileno(stdin),&c,1);
	
				if (bytes==1) {
					switch(c) {
					case 'q':
						if (manager->CheckForChangedImages()) {
							if (!input_yes_no("attention: there are changed images. quit anyway?")) {
								break;
							}
						}
						printf("good bye!\n");
						running = false;
						break;
					case ' ':
						print_status(manager);
						break;
					case 'h':
						printf("\n");
						printf("+-----------------------------------+\n");
						printf("|         atariserver help:         |\n");
						printf("+-----------------------------------+\n");
						printf("| h - help                          |\n");
						printf("| q - quit                          |\n");
						printf("| <space> - status                  |\n");
						printf("| c - create image                  |\n");
						printf("| l - load image                    |\n");
						printf("| w - write image(s) to file        |\n");
						printf("| u - unload image(s)               |\n");
						printf("| p - write protect disk(s)         |\n");
						printf("| P - unprotect disk(s)             |\n");
						printf("| x - exchange drives               |\n");
						printf("| d - display directory of image    |\n");
						printf("| s - set high speed SIO mode       |\n");
						printf("| a - auto status update on/off     |\n");
						printf("| t - set SIO trace level           |\n");
						printf("+-----------------------------------+\n");
						printf("\n");
						break;
					case 'p':
						{
							DeviceManager::EDriveNumber d = input_used_drive_number(manager, "write protect disk", "A(ll)");
							if (d != DeviceManager::eNoDrive) {
								if (manager->SetWriteProtectImage(d,true)) {
									print_ok();
									print_auto_status(manager);
								} else {
									print_error();
								}
							}
						}
						break;
					case 'P':
						{
							DeviceManager::EDriveNumber d = input_used_drive_number(manager,"unprotect disk", "A(ll)");
							if (d != DeviceManager::eNoDrive) {
								if (manager->SetWriteProtectImage(d,false)) {
									print_ok();
									print_auto_status(manager);
								} else {
									print_error();
								}
							}
						}
						break;

					case 'x':
						{
							DeviceManager::EDriveNumber d1,d2;
							print_drive_number_prompt("exchange drive", NULL);

							d1 = input_drive_number(NULL);
							if (d1 != DeviceManager::eNoDrive) {
								printf(" with ");
								fflush(stdout);
								d2 = input_drive_number(NULL);
								if (d2 != DeviceManager::eNoDrive) {
									if (d1 != d2) {
										if (manager->ExchangeDrives(d1,d2)) {
											print_ok();
											print_auto_status(manager);
										} else {
											print_error();
										}
									} else {
										printf(" - ignoring\n");
									}
								} else {
									print_aborted();
								}
							}
						}
						break;
					case 'u':
						{
							DeviceManager::EDriveNumber d = input_used_drive_number(manager, "unload drive", "A(ll)");
							if (d != DeviceManager::eNoDrive) {
								if (manager->UnloadDiskImage(d)) {
									print_ok();
									print_auto_status(manager);
								} else {
									print_error();
								}
							}
						}
						break;
					case 't':
						{
							printf("set SIO trace level [ 0-3 ESC ] > ");
							fflush(stdout);
							int d = input_char("0123");
							if (d >=0) {
									printf("%c",d);
									trace_level = d - '0';
									set_trace(trace_level);
									print_ok();
									print_auto_status(manager);
							} else {
									print_aborted();
							}
						}
						break;
					case 'l':
						{
							DeviceManager::EDriveNumber d = input_unused_drive_number(manager, "load image into drive", NULL);
							if (d != DeviceManager::eNoDrive) {
								printf("\n");
							  	char * filename = my_readline("filename > ");

							  	if (filename && *filename) {
									add_history(filename);

									if (manager->LoadDiskImage(d, filename)) {
										printf("loaded \"%s\" into D%d:\n", filename, d);
										print_auto_status(manager);
									} else {
										printf("error loading \"%s\"!\n",filename);
									}
								} else {
									print_aborted();
								}
								if (filename) {
									free (filename);
								}
							}
						}
						break;
					case 'w':
						{
							DeviceManager::EDriveNumber d = input_used_drive_number(manager, "write drive", "A(ll changed)");
							if (d != DeviceManager::eNoDrive) {
								if (d==DeviceManager::eAllDrives) {
									printf("\n");
									manager->WriteBackImagesIfChanged();
									print_auto_status(manager);
								} else {
									set_readline_default_text(manager->GetImageFilename(d));

									printf("\n");
									char * filename = my_readline("filename > ");

									if (filename && *filename) {
										add_history(filename);

										if (manager->WriteDiskImage(d, filename)) {
											printf("wrote D%d: to \"%s\"\n", d, filename);
											print_auto_status(manager);
										} else {
											printf("writing \"%s\" failed\n", filename);
										}
									} else {
										print_aborted();
									}
									if (filename) {
										free(filename);
									}
								}
							}
						}
						break;
					case 'c':
						{
							DeviceManager::EDriveNumber d = input_unused_drive_number(manager, "create image in drive", NULL);
							if (d != DeviceManager::eNoDrive) {
								printf("\nsize: [ 1=90k 2=130k 3=180k 4=360k S=other(SD) D=other(DD) ESC ] > ");
								fflush(stdout);
								int s = input_char("1234sSdD");
								if (s>=0) {
									printf("%c\n",s);
									switch (s) {
									case '1':
										if (manager->CreateAtrMemoryImage(d, e90kDisk)) {
											goto create_ok;
										} else {
											goto create_error;
										}
										break;
									case '2':
										if (manager->CreateAtrMemoryImage(d, e130kDisk)) {
											goto create_ok;
										} else {
											goto create_error;
										}
										break;
									case '3':
										if (manager->CreateAtrMemoryImage(d, e180kDisk)) {
											goto create_ok;
										} else {
											goto create_error;
										}
										break;
									case '4':
										if (manager->CreateAtrMemoryImage(d, e360kDisk)) {
											goto create_ok;
										} else {
											goto create_error;
										}
										break;
									case 's':
									case 'S':
									case 'd':
									case 'D':
										{
											char * sectors = my_readline("number of sectors: > ");

											if (sectors && *sectors) {
												int numSec = atoi(sectors);
												if (numSec < 1 || numSec > 65535) {
													printf("illegal number of sectors %d - aborted\n",numSec);
												} else {
													bool ret;
													if (toupper(s)=='S') {
														ret = manager->CreateAtrMemoryImage(d, e128BytesPerSector, numSec);
													} else {
														ret = manager->CreateAtrMemoryImage(d, e256BytesPerSector, numSec);
													}
													if (ret) {
														goto create_ok;
													} else {
														goto create_error;
													}
												}
											} else {
												print_aborted();
											}
										}
										break;
									create_ok:
										printf("created disk in D%d:\n",d);
										print_auto_status(manager);
										break;
									create_error:
										printf("error creating empty disk in D%d: !\n",d);
										break;
									}

								} else {
									print_aborted();
								}
							}
						}
						break;
					case 's':
						{
							printf("sio speed [ 0=slow 1=high 2=high/pause ESC ] > ");
							fflush(stdout);
							int sel = input_char("012");
							if (sel >= 0) {
								printf("%c", toupper(sel));
								switch (sel) {
								case '0':
									if (manager->SetHighSpeedMode(DeviceManager::eHighSpeedOff)) {
										print_ok();
									} else {
										print_error();
									}
									break;
								case '1':
									if (manager->SetHighSpeedMode(DeviceManager::eHighSpeedOn)) {
										print_ok();
									} else {
										print_error();
									}
									break;
								case '2':
									if (manager->SetHighSpeedMode(DeviceManager::eHighSpeedWithPause)) {
										print_ok();
									} else {
										print_error();
									}
									break;
								}
								print_auto_status(manager);
							} else {
								print_aborted();
							}
						}
						break;
					case 'a':
						{
							printf("enable auto status update: [ Y N ESC ] > ");
							fflush(stdout);
							int sel = input_char("yYnN");
							if (sel >= 0) {
								printf("%c", toupper(sel));
								if (toupper(sel) == 'Y') {
									auto_status_update = true;
								} else {
									auto_status_update = false;
								}
								print_ok();
								print_auto_status(manager);
							} else {
								print_aborted();
							}
						}
						break;
					case 'd':
						{
							DeviceManager::EDriveNumber d = input_used_drive_number(manager, "directory of drive", NULL);
							if (d != DeviceManager::eNoDrive) {
								printf("\n");
								RCPtr<AtrImage> atrImage = manager->GetAtrImage(d);
								if (atrImage) {
									print_directory(atrImage);
								} else {
									printf("cannot get ATR image!\n");
								}

							}
						}
						break;
					case 'K':
						{
								manager->GetSIOWrapper()->DebugKernelStatus();
								break;
						}
					default:
						printf("illegal command ");
						if (c>=32 && c<=126) {
							printf("'%c'",c);
						} else {
							printf("0x%02x",c);
						}
						printf(" - press 'h' for help\n");
						break;
					}
					fflush(stdout);
					fflush(stderr);
				} else {
					DPRINTF("read(STDIN) returned %d\n", bytes);
				}
			} else { // ret == -1
				DPRINTF("SIO manager returned error: %d\n",ret);
			}
		}
	}

	restore_stdin_mode();
	sioTracer->RemoveAllTracers();
	return 0;

usage:
	printf("usage: [-h] [-acCst] [ [-1] [-p] filename [ [-2] [-p] filename ...] ]\n");
	printf("-h          display help\n");
	printf("-a          disable auto status update\n");
	printf("-c          use alternative SIO2PC cable (command=DSR)\n");
	printf("-C          use alternative SIO2PC/nullmodem cable (command=CTS)\n");
	printf("-N          use SIO2PC cable without command line connected\n");
	printf("-p          write protect the next image\n");
	printf("-s          slow mode - disable highspeed SIO\n");
	printf("-S          high speed SIO mode with pauses\n");
	printf("-t          increase SIO trace level (default:0, max:3)\n");
	printf("-1..-8      set current drive number (default: 1)\n");
	printf("<filename>  load <filename> into current drive number, and then\n");
	printf("            increment drive number by one\n");
	return 1;
}

