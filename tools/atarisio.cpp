/*
   atarisio.cpp - all tools linked together into a single executable

   Copyright (C) 2005-2014 Matthias Reichl <hias@horus.com>

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
#include <string.h>

#include "OS.h"
#include "MiscUtils.h"
#include "Version.h"

extern int atariserver_main(int argc, char** argv);
extern int atarixfer_main(int argc, char** argv);
extern int adir_main(int argc, char** argv);
extern int dir2atr_main(int argc, char** argv);
extern int ataricom_main(int argc, char** argv);

#ifdef ENABLE_ATP
extern int atpdump_main(int argc, char** argv);
extern int atr2atp_main(int argc, char** argv);
#endif

typedef int (*main_type)(int, char**);

static main_type find_main(char* name, bool& drop_root)
{
	drop_root = false;
	if (strcmp(name,"atariserver") == 0) {
		return atariserver_main;
	}
	if (strcmp(name,"atarixfer") == 0) {
		return atarixfer_main;
	}
	drop_root = true;
	if (strcmp(name,"adir") == 0) {
		return adir_main;
	}
	if (strcmp(name,"dir2atr") == 0) {
		return dir2atr_main;
	}
	if (strcmp(name,"ataricom") == 0) {
		return ataricom_main;
	}
#ifdef ENABLE_ATP
	if (strcmp(name,"atpdump") == 0) {
		return atpdump_main;
	}
	if (strcmp(name,"atr2atp") == 0) {
		return atr2atp_main;
	}
#endif
	return NULL;
}

int main(int argc, char** argv)
{
	char* basename;
	main_type my_main;
	bool drop_root;

	basename = strrchr(argv[0], DIR_SEPARATOR);
	if (basename == 0) {
		basename = argv[0];
	} else {
		basename++;
	}

	my_main = find_main(basename, drop_root);

	if (!my_main) {
		if (argc == 1) {
			goto usage;
		}
		my_main = find_main(argv[1], drop_root);
		if (my_main) {
			argc--;
			argv++;
		}
	}
	if (my_main) {
		if (drop_root) {
			MiscUtils::drop_root_privileges();
		}
		return my_main(argc, argv);
	}

usage:
	printf("AtariSIO %s all-in-one package\n", VERSION_STRING);
	printf("(c) 2005-2014 Matthias Reichl <hias@horus.com>\n");
	printf("usage: atarisio atariserver|atarixfer|adir|dir2atr|ataricom");
#ifdef ENABLE_ATP
	printf("|atpdump|atr2atp");
#endif
	printf(" [...]\n");
	return 0;
}
