/*
   casinfo - print infos about CAS image

   (c) 2007-2010 Matthias Reichl <hias@horus.com>

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

#include "CasHandler.h"
#include "CasDataBlock.h"
#include "CasFskBlock.h"
#include "CasImage.h"
#include "SIOTracer.h"
#include "SIOWrapper.h"
#include "FileTracer.h"
#include "Version.h"

#include <stdio.h>

int main(int argc, char** argv)
{
	printf("casinfo " VERSION_STRING " (c) 2007-2010 Matthias Reichl\n");

	if (argc < 2) {
		printf("usage: casinfo file.cas ...\n");
		return 1;
	}

	SIOTracer* sioTracer = SIOTracer::GetInstance();
	{
		RCPtr<FileTracer> tracer(new FileTracer(stderr));
		sioTracer->AddTracer(tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceInfo, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceWarning, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceError, true, tracer);
		sioTracer->SetTraceGroup(SIOTracer::eTraceDebug, true, tracer);
	}


	int i;
	for (i=1; i<argc; i++) {
		const char* filename = argv[1];

		printf("infos for \"%s\":\n", filename);

		RCPtr<CasImage> image = new CasImage();

		if (!image->ReadImageFromFile(filename)) {
			printf("error reading file!\n");
			continue;
		}

		RCPtr<SIOWrapper> siowrapper;

		RCPtr<CasHandler> casHandler = new CasHandler(image, siowrapper);

		printf("Description: ");
		const char* desc = image->GetDescription();
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
			unsigned int baud = 0;
			bool is_fsk = false;

			if (block->IsDataBlock()) {
				baud = RCPtrStaticCast<CasDataBlock>(block)->GetBaudRate();
			} else {
				is_fsk = true;
			}

			if (block) {
				printf("%4d:  %s part: %2d  baud: %5d  gap: %5d  length: %5d\n",
					i, 
					is_fsk ? "fsk " : "data",
					block->GetPartNumber(),
					baud, block->GetGap(), block->GetLength());
			} else {
				printf("%4d:  ERROR getting block\n", i);
			}
		}
	}

	sioTracer->RemoveAllTracers();

	return 0;
}
