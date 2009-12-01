/*
   AtrMemoryImage.cpp - access ATR images in RAM.

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

#include "AtrMemoryImage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef USE_ZLIB
#include <zlib.h>
#endif

#include <ctype.h>

#include "DCMCodec.h"
#include "SIOTracer.h"
#include "AtariDebug.h"
#include "Dos2xUtils.h"
#include "MyPicoDosCode.h"

#include "winver.h"

AtrMemoryImage::AtrMemoryImage()
	: fData(0)
{
}

AtrMemoryImage::~AtrMemoryImage()
{
	FreeImageData();
}

void AtrMemoryImage::FreeImageData()
{
	if (fData) {
		delete[] fData;
		fData=0;
	}
	SetFormat(eNoDisk);
}

bool AtrMemoryImage::CreateImage(EDiskFormat format)
{
	size_t imgSize;
	FreeImageData();
	SetChanged(true);
	if (!SetFormat(format)) {
		DPRINTF("SetFormat failed");
		return false;
	}

	SetWriteProtect(false);

	if ((imgSize = GetImageSize()) == 0) {
		DPRINTF("GetImageSize = 0");
		return false;
	}
	fData = new uint8_t[imgSize];

	if (fData) {
		memset (fData, 0, imgSize);
		return true;
	} else {
		DPRINTF("cannot alloc fData");
		FreeImageData();
		return false;
	}
}

bool AtrMemoryImage::CreateImage(ESectorLength density, unsigned int sectors)
{
	size_t imgSize;
	FreeImageData();
	SetChanged(true);
	if (!SetFormat(density, sectors)) {
		DPRINTF("SetFormat failed");
		return false;
	}

	SetWriteProtect(false);

	if ((imgSize = GetImageSize()) == 0) {
		DPRINTF("GetImageSize = 0");
		return false;
	}
	fData = new uint8_t[imgSize];

	if (fData) {
		memset (fData, 0, imgSize);
		return true;
	} else {
		DPRINTF("cannot alloc fData");
		FreeImageData();
		return false;
	}
}

bool AtrMemoryImage::CreateImage(ESectorLength density, unsigned int sectorsPerTrack, unsigned int tracks, unsigned int sides)
{
	size_t imgSize;
	FreeImageData();
	SetChanged(true);
	if (!SetFormat(density, sectorsPerTrack, tracks, sides)) {
		DPRINTF("SetFormat failed");
		return false;
	}

	SetWriteProtect(false);

	if ((imgSize = GetImageSize()) == 0) {
		DPRINTF("GetImageSize = 0");
		return false;
	}
	fData = new uint8_t[imgSize];

	if (fData) {
		memset (fData, 0, imgSize);
		return true;
	} else {
		DPRINTF("cannot alloc fData");
		FreeImageData();
		return false;
	}
}


bool AtrMemoryImage::ReadImageFromFile(const char* filename, bool beQuiet)
{
	bool ret;
	EImageType imageType = DetermineImageTypeFromFilename(filename);

	if (imageType == eUnknownImageType) {
		if (!beQuiet) {
			ALOG("unknown image type - creating ATR image");
		}
	}

#ifndef USE_ZLIB
	switch (imageType) {
	case eAtrGzImageType:
	case eXfdGzImageType:
	case eDcmGzImageType:
	case eDiGzImageType:
		if (!beQuiet) {
			AERROR("cannot read compressed file - zlib support disabled at compiletime!");
		}
		return false;
	default: break;
	}
#endif

	switch (imageType) {
	case eAtrImageType:
	case eAtrGzImageType:
		ret = ReadImageFromAtrFile(filename, beQuiet);
		break;
	case eXfdImageType:
	case eXfdGzImageType:
		ret = ReadImageFromXfdFile(filename, beQuiet);
		break;
	case eDcmImageType:
	case eDcmGzImageType:
		ret = ReadImageFromDcmFile(filename, beQuiet);
		break;
	case eDiImageType:
	case eDiGzImageType:
		ret = ReadImageFromDiFile(filename, beQuiet);
		break;
	case eUnknownImageType:
		{
			char p[PATH_MAX];
			if (realpath(filename,p) == 0) {
				AERROR("realpath(\"%s\") failed", filename);
				return false;
			}
			strncat(p, ".atr", PATH_MAX);
			p[PATH_MAX-1] = 0;
			SetFilename(p);
			unsigned int sectors = Dos2xUtils::EstimateDiskSize(filename, e128BytesPerSector, Dos2xUtils::ePicoName);
			Dos2xUtils::EDosFormat dosformat = Dos2xUtils::eDos2x;
			if (sectors <= 1040) {
				// create standard ED image
				if (!CreateImage(e130kDisk)) {
					return false;
				}
			} else {
				// create custom MyDos image
				if (!CreateImage(e128BytesPerSector, sectors)) {
					return false;
				}
				dosformat = Dos2xUtils::eMyDos;
			}

			RCPtr<Dos2xUtils> dosutils = new Dos2xUtils(this,0,0);
			if (!dosutils->SetDosFormat(dosformat)) {
				AERROR("cannot set dos format");
				return false;
			}
			if (!dosutils->InitVTOC()) {
				AERROR("cannot init VTOC");
				return false;
			}
			if (!MyPicoDosCode::GetInstance()->WriteBootCodeToImage(this, true)) {
				AERROR("unable to write MyPicoDos boot sector code to image");
				return false;
			}
			if (!dosutils->AddFile(filename)) {
				return false;
			}
			dosutils->CreatePiconame(Dos2xUtils::ePicoName);
			SetChanged(false);
		}
		return true;
	default:
		if (!beQuiet) {
			DPRINTF("unsupported image type!");
		}
		return false;
	}
	return ret;
}

bool AtrMemoryImage::WriteImageToFile(const char* filename) const
{
	bool ret;
	EImageType imageType = DetermineImageTypeFromFilename(filename);

	if (imageType == eUnknownImageType) {
		AWARN("cannot determine image type from filename - using ATR format");
		imageType = eAtrImageType;
	}

#ifndef USE_ZLIB
	switch (imageType) {
	case eAtrGzImageType:
	case eXfdGzImageType:
	case eDcmGzImageType:
	case eDiGzImageType:
		AERROR("cannot write compressed file - zlib support disabled at compiletime!");
		return false;
	default: break;
	}
#endif

	switch (imageType) {
	case eAtrImageType:
	case eAtrGzImageType:
		ret = WriteImageToAtrFile(filename, imageType==eAtrGzImageType);
		break;
	case eXfdImageType:
	case eXfdGzImageType:
		ret = WriteImageToXfdFile(filename, imageType==eXfdGzImageType);
		break;
	case eDcmImageType:
	case eDcmGzImageType:
		ret = WriteImageToDcmFile(filename, imageType==eDcmGzImageType);
		break;
	case eDiImageType:
	case eDiGzImageType:
		ret = WriteImageToDiFile(filename, imageType==eDiGzImageType);
		break;
	case eUnknownImageType:
		DPRINTF("unknown image type!");
		return false;
	default:
		DPRINTF("unsupported image type!");
		return false;
	}
	return ret;
}

bool AtrMemoryImage::ReadImageFromAtrFile(const char* filename, bool beQuiet)
{
	uint8_t hdr[16];
	size_t imgSize, s;

	RCPtr<FileIO> fileio;

#ifdef USE_ZLIB
	fileio = new GZFileIO();
#else
	fileio = new StdFileIO();
#endif

	FreeImageData();

	if (!fileio->OpenRead(filename)) {
		if (!beQuiet) {
			AERROR("cannot open \"%s\" for reading",filename);
		}
		return false;
	}

	if ( fileio->ReadBlock(hdr, 16) != 16 ) {
		if (!beQuiet) {
			AERROR("cannot read ATR-header");
		}
		goto failure;
	}

	if (!SetFormatFromATRHeader(hdr)) {
		if (!beQuiet) {
			AERROR("illegal ATR header");
		}
		goto failure;
	}

	if ((imgSize = GetImageSize()) == 0) {
		if (!beQuiet) {
			DPRINTF("GetImageSize = 0");
		}
		goto failure;
	}
	fData = new uint8_t[imgSize];
	memset(fData, 0, imgSize);

	if (!fData) {
		if (!beQuiet) {
			DPRINTF("cannot alloc fData");
		}
		goto failure;
	}

	if ( (s=fileio->ReadBlock(fData, imgSize)) != imgSize) {
		if (!beQuiet) {
			AWARN("truncated ATR file: only got %d of %d bytes", (int)s, (int)imgSize);
		}
	}

	SetChanged(false);

	fileio->Close();
	return true;

failure:
	SetChanged(false);
	FreeImageData();
	fileio->Close();
	return false;
}

bool AtrMemoryImage::WriteImageToAtrFile(const char* filename, bool useGz) const
{
	uint8_t hdr[16];
	size_t imgSize;

	RCPtr<FileIO> fileio;

	if (! fData) {
		DPRINTF("no fData");
		return false;
	}

	if (!CreateATRHeaderFromFormat(hdr)) {
		DPRINTF("CreateATRHeaderFromFormat failed");
		return false;
	}

	if ((imgSize = GetImageSize()) == 0) {
		DPRINTF("GetImageSize = 0");
		return false;
	}

	if (useGz) {
#ifdef USE_ZLIB
		fileio = new GZFileIO();
#else
		AERROR("cannot write compressed file - zlib support disabled at compiletime!");
		return false;
#endif
	} else {
		fileio = new StdFileIO();
	}

	if (!fileio->OpenWrite(filename)) {
		AERROR("cannot open \"%s\" for writing",filename);
		return false;
	}
	if (fileio->WriteBlock(hdr,16) != 16) {
		AERROR("cannot write ATR header");
		goto failure;
	}
	if (fileio->WriteBlock(fData,imgSize) != imgSize) {
		AERROR("cannot write ATR image");
		goto failure;
	}
	SetChanged(false);
	fileio->Close();
	return true;

failure:
	fileio->Close();
	fileio->Unlink(filename);
	return false;
}

bool AtrMemoryImage::ReadImageFromXfdFile(const char* filename, bool beQuiet)
{
	size_t imgSize, s;
	uint32_t numSecs;
	ESectorLength seclen;

	RCPtr<FileIO> fileio;

#ifdef USE_ZLIB
	fileio = new GZFileIO();
#else
	fileio = new StdFileIO();
#endif

	FreeImageData();

	if (!fileio->OpenRead(filename)) {
		if (!beQuiet) {
			AERROR("cannot open \"%s\" for reading",filename);
		}
		return false;
	}

	imgSize = fileio->GetFileLength();

	if ( (imgSize & 0x7f) || imgSize < 384 ) {
		if (!beQuiet) {
			AERROR("illegal image size %d", (int)imgSize);
		}
		goto failure;
	}

	if (imgSize & 0x80) {
		seclen = e256BytesPerSector;
		numSecs = (imgSize - 384) / 256 + 3;
	} else {
		seclen = e128BytesPerSector;
		numSecs = imgSize / 128;
	}
	SetFormat(seclen, numSecs);

	if (imgSize != GetImageSize()) {
		if (!beQuiet) {
			DPRINTF("setting image size failed!");
		}
		goto failure;
	}
	fData = new uint8_t[imgSize];
	memset(fData, 0, imgSize);

	if (!fData) {
		if (!beQuiet) {
			DPRINTF("cannot alloc fData");
		}
		goto failure;
	}

	if ( (s=fileio->ReadBlock(fData, imgSize)) != imgSize) {
		if (!beQuiet) {
			AWARN("truncated XFD file: only got %d of %d bytes", (int)s, (int)imgSize);
		}
	}

	SetChanged(false);

	fileio->Close();
	return true;

failure:
	SetChanged(false);
	FreeImageData();
	fileio->Close();
	return false;
}

bool AtrMemoryImage::WriteImageToXfdFile(const char* filename, bool useGz) const
{
	size_t imgSize;
	RCPtr<FileIO> fileio;

	if (! fData) {
		DPRINTF("no fData");
		return false;
	}

	if ((imgSize = GetImageSize()) == 0) {
		DPRINTF("GetImageSize = 0");
		return false;
	}

	if (useGz) {
#ifdef USE_ZLIB
		fileio = new GZFileIO();
#else
		AERROR("cannot write compressed file - zlib support disabled at compiletime!");
		return false;
#endif
	} else {
		fileio = new StdFileIO();
	}

	if (!fileio->OpenWrite(filename)) {
		AERROR("cannot open \"%s\" for writing",filename);
		return false;
	}
	if (fileio->WriteBlock(fData,imgSize) != imgSize) {
		AERROR("cannot write XFD image");
		goto failure;
	}
	SetChanged(false);
	fileio->Close();
	return true;

failure:
	fileio->Close();
	fileio->Unlink(filename);
	return false;
}

bool AtrMemoryImage::ReadImageFromDcmFile(const char* filename, bool beQuiet)
{
	DCMCodec* dcmcodec;
	RCPtr<FileIO> fileio;

#ifdef USE_ZLIB
	fileio = new GZFileIO();
#else
	fileio = new StdFileIO();
#endif
       	dcmcodec = new DCMCodec(fileio, this);

	FreeImageData();

	bool ret;

	if ((ret=dcmcodec->Load(filename, beQuiet))) {
		SetChanged(false);
	}

	delete dcmcodec;
	return ret;
}

bool AtrMemoryImage::WriteImageToDcmFile(const char* filename, bool useGz) const
{
	DCMCodec* dcmcodec;
	RCPtr<FileIO> fileio;
	if (useGz) {
#ifdef USE_ZLIB
		fileio = new GZFileIO();
#else
		AERROR("cannot write compressed file - zlib support disabled at compiletime!");
		return 1;
#endif
	} else {
		fileio = new StdFileIO();
	}

	dcmcodec = new DCMCodec(fileio, const_cast<AtrMemoryImage*>(this));

	bool ret;

	if ((ret=dcmcodec->Save(filename))) {
		SetChanged(false);
	}

	delete dcmcodec;
	return ret;
}

uint8_t AtrMemoryImage::CalculateDiSectorChecksum(uint8_t* buf, unsigned int len) const
{
	uint16_t chksum = 0;
	unsigned int i;
	for (i=0;i<len;i++) {
		chksum += buf[i];
		if (chksum >= 256) {
			chksum -= 255;
		}
	}
	return chksum;
}

bool AtrMemoryImage::ReadImageFromDiFile(const char* filename, bool beQuiet)
{
	uint8_t buf[256];
	uint8_t* map;
	uint8_t unknown;
	uint16_t sectorsPerTrack;
        uint8_t tracksPerSide;
	uint8_t sides;
	uint16_t sectorLength;
	unsigned int totalSectors;
	ESectorLength density;

	unsigned int sector;

	RCPtr<FileIO> fileio;

#ifdef USE_ZLIB
	fileio = new GZFileIO();
#else
	fileio = new StdFileIO();
#endif

	FreeImageData();

	if (!fileio->OpenRead(filename)) {
		if (!beQuiet) {
			AERROR("cannot open \"%s\" for reading",filename);
		}
		return false;
	}

	{
		// check header
		if (fileio->ReadBlock(buf,2) != 2) {
			goto failure_eof;
		}
		if (buf[0] != 'D' || buf[1] != 'I') {
			if (!beQuiet) {
				AERROR("not an DI file!");
			}
			goto failure;
		}

		// check version
		uint16_t version;
		if (!fileio->ReadWord(version)) {
			goto failure_eof;
		}
		if (version != 0x0220) {
			if (!beQuiet) {
				AERROR("illegal version in DI image: %04x",version);
			}
			goto failure;
		}

		// skip creator string
		bool ok;

		do {
			ok = fileio->ReadByte(buf[0]);
		} while (ok && buf[0]);

		if (!ok) {
			goto failure_eof;
		}
	}

	// read image header

	if (!fileio->ReadByte(unknown)) {
		goto failure_eof;
	}

	if (!fileio->ReadByte(tracksPerSide)) {
		goto failure_eof;
	}

	if (!fileio->ReadBigEndianWord(sectorsPerTrack)) {
		goto failure_eof;
	}

	if (!fileio->ReadByte(sides)) {
		goto failure_eof;
	}
	sides++;

	if (!fileio->ReadByte(unknown)) {
		goto failure_eof;
	}

	if (!fileio->ReadBigEndianWord(sectorLength)) {
		goto failure_eof;
	}

	if (!fileio->ReadByte(unknown)) {
		goto failure_eof;
	}

	totalSectors = sectorsPerTrack * tracksPerSide * sides;

	if (totalSectors == 0 || totalSectors > 65535) {
		if (!beQuiet) {
			AERROR("illegal number of sectors in DI image header: %d", totalSectors);
		}
		goto failure;
	}

	switch (sectorLength) {
	case 128:
		density = e128BytesPerSector;
		break;
	case 256:
		density = e256BytesPerSector;
		break;
	default:
		if (!beQuiet) {
			AERROR("illegal sector length in DI image header: %d", sectorLength);
		}
		goto failure;
	}

	if (!CreateImage(density, sectorsPerTrack, tracksPerSide, sides)) {
		goto failure;
	}

	// read sector (checksum) map
       	map = new uint8_t[totalSectors];

	if (fileio->ReadBlock(map, totalSectors) != totalSectors) {
		delete [] map;
		goto failure_eof;
	}

	for (sector=1; sector <= totalSectors; sector++) {
		uint8_t checksum = map[sector-1];

		if (checksum) {
			unsigned int len = GetSectorLength(sector);
			if (fileio->ReadBlock(buf, len) != len) {
				delete [] map;
				goto failure_eof;
			}
			if (CalculateDiSectorChecksum(buf, len) != checksum) {
				if (!beQuiet) {
					AERROR("checksum error in DI image at sector %d",sector);
				}
				delete [] map;
				goto failure;
			}
			if (!WriteSector(sector, buf, len)) {
				if (!beQuiet) {
					DPRINTF("error storing sector %d in internal image!", sector);
				}
				delete [] map;
				goto failure;
			}
		}
	}

	delete [] map;

	SetChanged(false);

	fileio->Close();
	return true;

failure_eof:
	if (!beQuiet) {
		AERROR("error reading DI file: unexpected EOF");
	}

failure:
	SetChanged(false);
	FreeImageData();
	fileio->Close();
	return false;
}

bool AtrMemoryImage::WriteImageToDiFile(const char* filename, bool useGz) const
{
	uint16_t sectorsPerTrack;
        uint8_t tracksPerSide;
	uint8_t sides;
	uint16_t sectorLength;
	unsigned int totalSectors;
	unsigned int sector;
	uint8_t* map;
	uint8_t buf[256];

	RCPtr<FileIO> fileio;

	if (! fData) {
		DPRINTF("no fData");
		return false;
	}

	if ( GetSectorsPerTrack() < 1
	  //|| GetSectorsPerTrack() > 65535
	  || GetTracksPerSide() < 1
	  //|| GetTracksPerSide() > 255
	  || GetSides() < 1
	  //|| GetSides() > 255
	) {
		AERROR("image format incomatible with DI format!");
		return false;
	}
	sectorsPerTrack = GetSectorsPerTrack();
	tracksPerSide = GetTracksPerSide();
	sides = GetSides() - 1;
	totalSectors = GetNumberOfSectors();
	sectorLength = GetSectorLength();

	if (useGz) {
#ifdef USE_ZLIB
		fileio = new GZFileIO();
#else
		AERROR("cannot write compressed file - zlib support disabled at compiletime!");
		return false;
#endif
	} else {
		fileio = new StdFileIO();
	}

	if (!fileio->OpenWrite(filename)) {
		AERROR("cannot open \"%s\" for writing",filename);
		return false;
	}

	map = new uint8_t [totalSectors];

	// build sector map
	for (sector=1; sector<=totalSectors; sector++) {
		uint16_t len = GetSectorLength(sector);
		if (!ReadSector(sector, buf, len)) {
			DPRINTF("unable to read sector %d from memory image!",sector);
			goto failure;
		}
		map[sector-1] = CalculateDiSectorChecksum(buf, len);
	}

	// creator information
	strcpy((char*)buf,"AtariSIO");

	if ( !fileio->WriteByte('D')
	  || !fileio->WriteByte('I')
	  || !fileio->WriteWord(0x0220)
	  || fileio->WriteBlock(buf, strlen((char*)buf)+1) != strlen((char*)buf)+1
	  || !fileio->WriteByte(1)
	  || !fileio->WriteByte(tracksPerSide)
	  || !fileio->WriteBigEndianWord(sectorsPerTrack)
	  || !fileio->WriteByte(sides)
	  || !fileio->WriteByte(0)
	  || !fileio->WriteBigEndianWord(sectorLength)
	  || !fileio->WriteByte(0)
	  || fileio->WriteBlock(map, totalSectors) != totalSectors) {
		goto failure_eof;
	}

	// write non-zero sectors
	for (sector=1; sector<=totalSectors; sector++) {
		if (map[sector-1]) {
			unsigned int len = GetSectorLength(sector);
			if (!ReadSector(sector, buf, len)) {
				DPRINTF("unable to read sector %d from memory image!",sector);
				goto failure;
			}
			if (fileio->WriteBlock(buf, len) != len) {
				goto failure_eof;
			}
		}
	}

	delete [] map;
	SetChanged(false);
	fileio->Close();
	return true;

failure_eof:
	AERROR("disk full");

failure:
	delete [] map;
	fileio->Close();
	fileio->Unlink(filename);
	return false;
}

bool AtrMemoryImage::ReadSector(unsigned int sector,uint8_t* buffer,unsigned int buffer_length) const
{
	bool ret=true;
	unsigned int len;
	ssize_t offset;

	if ((offset=CalculateOffset(sector)) < 0 ) {
		DPRINTF("illegal sector in ReadSector: %d", sector);
		return false;
	}

	len=GetSectorLength(sector);

	if (!buffer_length) {
		DPRINTF("buffer length = 0");
		return false;
	}

	if (buffer_length < len) {
		DPRINTF("buffer length < sector length [ %d < %d ]",buffer_length, len);
		ret = false;
		len = buffer_length;
	} else if (buffer_length > len) {
		DPRINTF("buffer length > sector length [ %d > %d ]",buffer_length, len);
		ret = false;
	}

	memcpy(buffer, fData+offset, len);

	return ret;
}

bool AtrMemoryImage::WriteSector(unsigned int sector, const uint8_t* buffer, unsigned int buffer_length)
{
	unsigned int len;
	ssize_t offset;

	if (IsWriteProtected()) {
		DPRINTF("attempting to write sector to write protected image");
		return false;
	}

	if ((offset=CalculateOffset(sector)) < 0 ) {
		DPRINTF("illegal sector in WriteSector: %d", sector);
		return false;
	}

	len=GetSectorLength(sector);

	if (buffer_length != len) {
		DPRINTF("buffer length = len [ %d != %d ]", buffer_length, len);
		return false;
	}

	SetChanged(true);
	memcpy(fData+offset, buffer, len);

	return true;
}

bool AtrMemoryImage::IsAtrMemoryImage() const
{
	return true;
}

void AtrMemoryImage::SetWriteProtect(bool on)
{
	//if (on != IsWriteProtected()) {
	//	SetChanged(true);
	//}
	super::SetWriteProtect(on);
}

AtrMemoryImage::EImageType AtrMemoryImage::DetermineImageTypeFromFilename(const char* filename) const
{
	size_t len = strlen(filename);

	if (len < 3) {
		return eUnknownImageType;
	}
	if (strcasecmp(filename+len-3,".di") == 0) {
		return eDiImageType;
	}

	if (len < 4) {
		return eUnknownImageType;
	}
	if (strcasecmp(filename+len-4,".atr") == 0) {
		return eAtrImageType;
	}
	if (strcasecmp(filename+len-4,".xfd") == 0) {
		return eXfdImageType;
	}
	if (strcasecmp(filename+len-4,".dcm") == 0) {
		return eDcmImageType;
	}

	if (len < 6) {
		return eUnknownImageType;
	}
	if (strcasecmp(filename+len-6,".di.gz") == 0) {
		return eDiGzImageType;
	}

	if (len < 7) {
		return eUnknownImageType;
	}
	if (strcasecmp(filename+len-7,".atr.gz") == 0) {
		return eAtrGzImageType;
	}
	if (strcasecmp(filename+len-7,".xfd.gz") == 0) {
		return eXfdGzImageType;
	}
	if (strcasecmp(filename+len-7,".dcm.gz") == 0) {
		return eDcmGzImageType;
	}

	return eUnknownImageType;
}

