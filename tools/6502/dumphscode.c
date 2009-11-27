/*
   dumphscode.c - create C++ source files from 6502 highspeed SIO
   object code

   Copyright (C) 2003 Matthias Reichl <hias@horus.com>

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

static char objfile[]="atarisio-highsio.bin";

int main(void)
{

	FILE *in;
	FILE *out;
	uint8_t hdr[6];
	uint8_t* buf;
	int len, objlen, rellen, origstart, origend;
	int i;

	if (!(in=fopen(objfile,"rb"))) {
		printf("error opening %s\n",objfile);
		return 1;
	}

	len=fread(hdr,1,6,in);
	if (len!=6) {
		printf("error reading header of %s\n",objfile);
		return 1;
	}
	origstart = hdr[2]+256*hdr[3];
	origend = hdr[4]+256*hdr[5];
	if (hdr[0] != 0xff || hdr[1] != 0xff || origstart>origend) {
		printf("illegal file header in %s!\n", objfile);
		return 1;
	}

	buf = (uint8_t*) malloc(origend-origstart+1);
	len=fread(buf,1,origend-origstart+1,in);

	fclose(in);
	if (len < 2 || len != origend-origstart+1) {
		printf("illegal file %s\n", objfile);
		return 1;
	}

	fprintf(stderr, "read %d bytes\n", len);
	rellen = buf[len-2] + 256*buf[len-1];
	objlen = len - 2 - rellen;
	rellen = rellen / 2;

	fprintf(stderr, "original address was 0x%4x\n", origstart);
	fprintf(stderr, "obj len is %d\n", objlen);
	fprintf(stderr, "relocator table len is %d\n", rellen);

	if (!(out = fopen("atarisio-highsio.h","wb"))) {
		printf("cannot create atarisio-highsio.h\n");
		return 1;
	}
	fprintf(out,"enum { eSIOCodeLength = %d,\n",objlen);
        fprintf(out,"       eRelocatorLength = %d,\n",rellen);
	fprintf(out,"       eOriginalAddress = 0x%04x\n",origstart);
	fprintf(out,"};\n");
	fclose(out);

	printf("successfully created atarisio-highsio.h\n");

	if (!(out = fopen("atarisio-highsio.c","wb"))) {
		printf("cannot create atarisio-highsio.c\n");
		return 1;
	}
	fprintf(out,"uint8_t HighSpeedSIOCode::fSIOCode[] = {");
	for (i=0;i<objlen;i++) {
		if (i % 12 == 0) {
			fprintf(out,"\n\t");
		}
		fprintf(out,"0x%02x, ", buf[i]);
	}
	fprintf(out,"\n};\n\n");
	fprintf(out,"unsigned int HighSpeedSIOCode::fRelocatorTable[] = {");
	for (i=0;i<rellen;i++) {
		if (i % 8 == 0) {
			fprintf(out,"\n\t");
		}
		fprintf(out,"0x%04x, ", (buf[i*2+objlen]+256*buf[i*2+objlen+1]) - origstart);
	}
	fprintf(out,"\n};\n");
	fclose(out);

	printf("successfully created atarisio-highsio.c\n");

	return 0;
}
