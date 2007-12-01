/*
   casinfo - print infos about CAS image

   (c) 2007 Matthias Reichl <hias@horus.com>

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

#include "CasImage.h"
#include "SIOTracer.h"
#include "FileTracer.h"

#include <stdio.h>

int main(int argc, char** argv)
{
	if (argc < 2) {
		printf("usage: casinfo file.cas\n");
		return 1;
	}

	SIOTracer* sioTracer = SIOTracer::GetInstance();
	{
		RCPtr<FileTracer> tracer(new FileTracer(stderr));
		sioTracer->AddTracer(tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceInfo, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceDebug, true, tracer);
	}

	const char* filename = argv[1];

	RCPtr<CasImage> image = new CasImage();

	if (!image->ReadImageFromFile(filename)) {
		printf("error reading CAS file %s\n", filename);
		return 1;
	}

	printf("Description: ");
	char* desc = image->GetDescription();
	if (desc) {
		printf("%s\n", desc);
	} else {
		printf("<none>\n");
	}

	unsigned int total_blocks = image->GetNumberOfBlocks();

	printf("Number of Parts: %d\n", image->GetNumberOfParts());
	printf("Number of Blocks: %d\n", total_blocks);

	unsigned int i;
	for (i=0; i<total_blocks; i++) {
		RCPtr<CasBlock> block = image->GetBlock(i);

		if (block) {
			printf("%4d:  part: %2d  baud: %5d  gap: %5d  length: %5d\n",
				i, 
				block->GetPartNumber(),
				block->GetBaudRate(), block->GetGap(), block->GetLength());
		} else {
			printf("%4d:  ERROR getting block\n", i);
		}
	}

	sioTracer->RemoveAllTracers();

	return 0;
}
