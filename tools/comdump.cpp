/*
   comdump - dump blocks of a Atari COM file

   Copyright (C) 2008 Matthias Reichl <hias@horus.com>

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
#include <string.h>
#include "AtariComMemory.h"
#include "ComBlock.h"
#include "Error.h"
#include "Version.h"

int main(int argc, char** argv)
{
	FILE* f;
	FILE* of;
	char* filename;
	char* out_filename;
	bool done = false;
	int blkcount = 0;

	bool mergeMode = false;
	int merge_start;
	int merge_end;

	RCPtr<AtariComMemory> memory;

	printf("comdump V" VERSION_STRING " (c) 2008 by Matthias Reichl <hias@horus.com>\n");
	if (argc < 2) {
		goto usage;
	}
	if (strcmp(argv[1], "-m") == 0) {
		mergeMode = true;
		if (argc < 6) {
			goto usage;
		}
		merge_start = atoi(argv[2]);
		merge_end = atoi(argv[3]);
		filename = argv[4];
		out_filename = argv[5];
		if (merge_start < 0 || merge_end < 0 || (merge_end <= merge_start)) {
			goto usage;
		}
		memory = new AtariComMemory();

		printf("using merge mode: blk %d - %d\n", merge_start, merge_end);
	} else {
		filename = argv[1];
	}

	if (!(f = fopen(filename,"rb"))) {
		printf("cannot open %s\n", filename);
		return 1;
	}
	done = false;

	if (mergeMode) {
		if (!(of = fopen(out_filename,"wb"))) {
			printf("cannot create output file %s\n", out_filename);
			return 1;
		}
	}

	while (!done) {
		RCPtr<ComBlock> block;

		try {
			block = new ComBlock(f);
		}
		catch (EOFError) {
			printf("got end of file\n");
			done = true;
		}
		catch (ReadError) {
			printf("error reading file %s, terminating\n", filename);
			done = true;
		}
		catch (ErrorObject& e) {
			printf("%s\n", e.AsString());
			done = true;
		}
		if (!done) {
			printf("block %04d: %04X-%04X (%d bytes, offset %ld)\n",
				blkcount,
				block->GetStartAddress(),
				block->GetEndAddress(),
				block->GetLength(),
				block->GetFileOffset());
			if (block->ContainsAddress(0x2e0) && block->ContainsAddress(0x2e1)) {
				printf("       RUN: %02X%02X\n",
					block->GetByte(0x2e1),
					block->GetByte(0x2e0));
			}
			if (block->ContainsAddress(0x2e2) && block->ContainsAddress(0x2e3)) {
				printf("      INIT: %02X%02X\n",
					block->GetByte(0x2e3),
					block->GetByte(0x2e2));
			}
			if (mergeMode) {
				if (blkcount >= merge_start && blkcount <= merge_end) {
					printf("merging block %d\n", blkcount);
					memory->WriteComBlockToMemory(block);

					if (blkcount == merge_end) {
						RCPtr<ComBlock> mblock = memory->AsComBlock();
						printf("writing block %04X-%04X\n",
								mblock->GetStartAddress(),
								mblock->GetEndAddress());
						memory->Clear();
						if (!mblock->WriteToFile(of, blkcount==0 )) {
							printf("error writing to output file\n");
							done = true;
						}
					}
				} else {
					if (!block->WriteToFile(of, blkcount==0 )) {
						printf("error writing to output file\n");
						done = true;
					}
				}
			}
			blkcount++;
		}
	}
	if (mergeMode) {
		if (memory->ContainsData()) {
			printf("Warning: EOF reached but still data to write.\n");
			RCPtr<ComBlock> mblock = memory->AsComBlock();
			if (!mblock->WriteToFile(of, blkcount==0 )) {
				printf("error writing to output file\n");
			}
		}
		fclose(of);
	}
	return 0;

usage:
	printf("usage: comdump filename\n");
	printf("   or: comdump -m start end filename out-filename\n");
	return 1;
}
