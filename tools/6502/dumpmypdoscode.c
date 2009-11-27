/*
   dumpmypdos.c - create C++ source files from MyPicoDos boot image

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
#include <stdlib.h>
#include <stdint.h>

static char objfile[]="atarisio-mypdos.bin";
static char bootfile[]="bootloader.bin";

static void dump_buf(FILE* out, uint8_t* buf, int len)
{
	int i;
	for (i=0;i<len;i++) {
		if (i % 12 == 0) {
			fprintf(out,"\n\t");
		}
		fprintf(out,"0x%02x, ", buf[i]);
	}
}

int main(void)
{

	FILE *in;
	FILE *out;
	uint8_t buf[16384];
	uint8_t bootbuf[384];

	int len, bootlen;

	if (!(in=fopen(objfile,"rb"))) {
		printf("error opening %s\n",objfile);
		return 1;
	}

	len=fread(buf,1,16384,in);
	fclose(in);
	if ((len == 0) || (len & 0x7f)) {
		printf("error reading %s\n",objfile);
		return 1;
	}

	fprintf(stderr, "MyPicoDos: read %d bytes\n", len);

	if (!(in=fopen(bootfile,"rb"))) {
		printf("error opening %s\n",objfile);
		return 1;
	}

	bootlen=fread(bootbuf,1,384,in);
	fclose(in);
	if (bootlen != 384) {
		printf("error reading %s\n",bootfile);
		return 1;
	}

	fprintf(stderr, "boot loader: read %d bytes\n", bootlen);

	if (!(out = fopen("mypicodoscode.h","wb"))) {
		printf("cannot create mypicodoscode.h\n");
		return 1;
	}
	fprintf(out,"enum { eCodeLength = %d };\n",len);
	fprintf(out,"enum { eBootLength = %d };\n",bootlen);
	fclose(out);

	printf("successfully created mypicodoscode.h\n");

	if (!(out = fopen("mypicodoscode.c","wb"))) {
		printf("cannot create mypicodoscode.c\n");
		return 1;
	}
	fprintf(out,"const uint8_t MyPicoDosCode::fCode[] = {");
	dump_buf(out, buf, len);
	fprintf(out,"\n};\n\n");
	fprintf(out,"const uint8_t MyPicoDosCode::fBootCode[] = {");
	dump_buf(out, bootbuf, bootlen);
	fprintf(out,"\n};\n\n");


	fclose(out);

	printf("successfully created mypicodoscode.c\n");

	return 0;
}
