/*
   CasImage - CAS Image class

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
#include "CasDataBlock.h"
#include "CasFskBlock.h"
#include "AtariDebug.h"
#include "FileIO.h"
#include <string.h>
#include <list>

CasImage::CasImage()
	: fNumberOfBlocks(0),
	  fNumberOfParts(0),
	  fCasBlocks(0),
	  fDescription(0),
	  fFilename(0),
	  fTempBuf(0)
{
}

CasImage::~CasImage()
{
	FreeData();
	FreeTempBuf();
}

void CasImage::FreeData()
{
	if (fCasBlocks) {
		delete[] fCasBlocks;
		fCasBlocks = 0;
	}
	if (fDescription) {
		delete[] fDescription;
		fDescription = 0;
	}
	if (fFilename) {
		delete[] fFilename;
		fFilename = 0;
	}
	fNumberOfBlocks = 0;
	fNumberOfParts = 0;
}

CasImage::EBlockType CasImage::ReadBlockFromFile(
	RCPtr<FileIO> fileio,
	unsigned int& length,
	unsigned int& aux,
	bool readUnknownBlockData)
{
	uint8_t buf[8];

	unsigned int tmp;
	tmp = fileio->ReadBlock(buf, 8);
	if (tmp == 0) {
		return eBlockEOF;
	}
	if (tmp != 8) {
		return eBlockIOError;
	}

	length = buf[4] + (buf[5] << 8);
	aux = buf[6] + (buf[7] << 8);

	EBlockType type = eUnknownBlock;

	if (memcmp(buf, "FUJI", 4) == 0) {
		type = eFujiBlock;
	}

	if (memcmp(buf, "baud", 4) == 0) {
		type = eBaudBlock;
	}

	if (memcmp(buf, "data", 4) == 0) {
		type = eDataBlock;
	}

	if (memcmp(buf, "fsk ", 4) == 0) {
		type = eFskBlock;
	}

	if (length && (type != eUnknownBlock || readUnknownBlockData) ) {
		tmp = fileio->ReadBlock(fTempBuf, length);
		if (tmp != length) {
			return eBlockIOError;
		}
	}

	if (type == eUnknownBlock) {
		for (tmp=0; tmp<4; tmp++) {
			if (buf[tmp] < 32 || buf[tmp] > 127) {
				buf[tmp] = '?';
			}
		}
		buf[4] = 0;
		AWARN("unknown block type \"%s\"", buf);
	}
	return type;
}

bool CasImage::ReadImageFromFile(const char* filename)
{
	RCPtr<FileIO> fileio;
	unsigned int baudrate = 600; // default baud rate

#ifdef USE_ZLIB
	fileio = new GZFileIO();
#else
	fileio = new StdFileIO();
#endif

	FreeData();

	if (!fileio->OpenRead(filename)) {
		AERROR("cannot open \"%s\" for reading",filename);
		return false;
	}

	AllocTempBuf();

	EBlockType blocktype;
	unsigned int length;
	unsigned int aux;

	unsigned int partno = 0;
	bool done = false;

	std::list< RCPtr<CasBlock> > blocklist;


	blocktype = ReadBlockFromFile(fileio, length, aux, false);

	if (blocktype != eFujiBlock) {
		AERROR("\"%s\" doesn't start with FUJI header",filename);
		goto exit_error;
	}
	if (length) {
		SetDescription(fTempBuf, length);
	}

	while (!done) {
		blocktype = ReadBlockFromFile(fileio, length, aux);
		switch (blocktype) {
		case eFujiBlock:
			AERROR("found another FUJI block in \"%s\"", filename);
			goto exit_error;
		case eBaudBlock:
			//ALOG("baud block: %d", aux);
			baudrate = aux;
			break;
		case eBlockEOF:
			//ALOG("got EOF");
			done = true;
			break;
		case eBlockIOError:
			AERROR("error reading \"%s\"", filename);
			goto exit_error;
		case eDataBlock:
			{
				//ALOG("data block: gap: %d len: %d", aux, length);
				RCPtr<CasDataBlock> block;
				if (fNumberOfBlocks > 0 && aux > 5000) {
					partno++;
				}
				block = new CasDataBlock(aux, length, fTempBuf, baudrate, partno);
				blocklist.push_back(block);
				fNumberOfBlocks++;
			}
			break;
		case eFskBlock:
			{
				//ALOG("data block: gap: %d len: %d", aux, length);
				RCPtr<CasFskBlock> block;
				if (fNumberOfBlocks > 0 && aux > 5000) {
					partno++;
				}
				block = new CasFskBlock(aux, length, fTempBuf, partno);
				blocklist.push_back(block);
				fNumberOfBlocks++;
			}
			break;
		case eUnknownBlock:
			break;
		}
	}

	fNumberOfParts = partno + 1;

	if (blocklist.size() == 0) {
		AERROR("No data blocks in \"%s\"", filename);
		goto exit_error;
	}
	fCasBlocks = new RCPtr<CasBlock>[fNumberOfBlocks];

	{
		unsigned int i = 0;
		std::list< RCPtr<CasBlock> >::const_iterator iter;
		std::list< RCPtr<CasBlock> >::const_iterator listend = blocklist.end();
		for (iter=blocklist.begin(); iter != listend; iter++) {
			fCasBlocks[i++] = *iter;
		}
		Assert(i == fNumberOfBlocks);
	}

	fileio->Close();
	FreeTempBuf();

	fFilename = new char[strlen(filename)+1];
	strcpy (fFilename, filename);

	return true;

exit_error:
	FreeData();
	FreeTempBuf();
	fileio->Close();
	return false;
}

void CasImage::AllocTempBuf()
{
	FreeTempBuf();
	fTempBuf = new uint8_t[65536];
}

void CasImage::FreeTempBuf()
{
	if (fTempBuf) {
		delete[] fTempBuf;
		fTempBuf = 0;
	}
}

void CasImage::SetDescription(uint8_t* str, unsigned int length)
{
	if (fDescription) {
		delete[] fDescription;
		fDescription = 0;
	}
	if (length) {
		fDescription = new char[length+1];
		memcpy(fDescription, str, length);
		fDescription[length] = 0;
	}
}

RCPtr<CasBlock> CasImage::GetBlock(unsigned int blockno)
{
	if (blockno < fNumberOfBlocks) {
		return fCasBlocks[blockno];
	} else {
		return RCPtr<CasBlock>();
	}
}
