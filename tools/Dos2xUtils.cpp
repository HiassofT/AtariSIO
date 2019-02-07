/*
   Dos2xUtils - utilities to access DOS 2.x images

   Copyright (C) 2004-2009 Matthias Reichl <hias@horus.com>

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
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "OS.h"
#include "Dos2xUtils.h"
#include "AtariDebug.h"
#include "Directory.h"
#include "MiscUtils.h"
using namespace MiscUtils;
#include "VirtualImageObserver.h"
#include "MyPicoDosCode.h"
#include "SIOTracer.h"
#include "FileIO.h"

#include "winver.h"

#include "DosBootCode.cpp"

#define MAX_FILE_LEN (16*1024*1024)

Dos2xUtils::Dos2xUtils(RCPtr<DiskImage> img,
		const char* directory,
		RCPtr<VirtualImageObserver> observer)
	: fDirSector(eDirSector),
	  fEntryCount(0),
	  fImage(img.GetRealPointer()),
	  fObserver(observer.GetRealPointer()),
	  fDosFormat(eUnknownDos),
	  fIsDos25EnhancedDensity(false),
	  fUse16BitSectorLinks(false),
	  fNumberOfVTOCs(0),
	  fIsRootDirectory(false)
{
	Assert(fImage);

	if (directory) {
		int len = strlen(directory);
		fDirectory = new char[len+1];
		if (len) {
			strcpy(fDirectory, directory);
			if ((len == 1) && (*fDirectory == DIR_SEPARATOR)) {
				fIsRootDirectory = true;
			} else {
				if (fDirectory[len-1] == DIR_SEPARATOR) {
					fDirectory[len-1] = 0;
				}
			}
		} else {
			*fDirectory = 0;
		}
	} else {
		fDirectory = 0;
	}

	int i;
	for (i=0;i<eMaxEntries;i++) {
		fOrigName[i] = 0;
		fAtariName[i] = 0;
	}
	if (fObserver) {
		fObserver->SetDirectoryObserver(fDirSector, this);
	}
}

Dos2xUtils::Dos2xUtils( DiskImage* img,
		const char* directory,
		VirtualImageObserver* observer,
		unsigned int dirSector,
		EDosFormat format,
		bool isDos25ED,
		bool use16BitLinks,
		unsigned int numVTOC)
	: fDirSector(dirSector),
	  fEntryCount(0),
	  fImage(img),
	  fObserver(observer),
	  fDosFormat(format),
	  fIsDos25EnhancedDensity(isDos25ED),
	  fUse16BitSectorLinks(use16BitLinks),
	  fNumberOfVTOCs(numVTOC),
	  fIsRootDirectory(false)
{
	Assert(fImage);
	Assert(directory);

	int len = strlen(directory);
	fDirectory = new char[len+1];
	if (len) {
		strcpy(fDirectory, directory);
		if ((len == 1) && (*fDirectory == DIR_SEPARATOR)) {
			fIsRootDirectory = true;
		} else {
			if (fDirectory[len-1] == DIR_SEPARATOR) {
				fDirectory[len-1] = 0;
			}
		}
	} else {
		*fDirectory = 0;
	}
	fEntryCount = 0;

	int i;
	for (i=0;i<eMaxEntries;i++) {
		fOrigName[i] = 0;
		fAtariName[i] = 0;
	}
	if (fObserver) {
		fObserver->SetDirectoryObserver(fDirSector, this);
	}
}

Dos2xUtils::~Dos2xUtils()
{
	if (fObserver) {
		fObserver->RemoveDirectoryObserver(fDirSector, this);
	}
	if (fDirectory) {
		delete[] fDirectory;
	}
	int i;
	for (i=0;i<eMaxEntries;i++) {
		if (fOrigName[i]) {
			delete[] fOrigName[i];
		}
		if (fAtariName[i]) {
			delete[] fAtariName[i];
		}
	}
}

bool Dos2xUtils::AddFiles(EPicoNameType piconametype)
{
	bool ok = true;
	if (!fDirectory) {
		DPRINTF("AddFiles: directory is NULL");
		return false;
	}
	RCPtr<Directory> dir = new Directory();
	int num = dir->ReadDirectory(fDirectory, true);
	//DPRINTF("ReadDirectory(\"%s\") returned %d", fDirectory, num);
	bool recurseSubdirs = (GetDosFormat() == eMyDos);
	int i;
	unsigned int entryNum;
	unsigned int dirsec;
	for (i=fEntryCount ; (fEntryCount < eMaxEntries) && i<num ; i++) {
		DirEntry* e = dir->Get(i);
		//DPRINTF("AddFiles(%s): processing %s", fDirectory, e->fName);
		switch (e->fType) {
		case DirEntry::eFile:
			if (!AddFile(e->fName)) {
				ok = false;
			}
			break;
		case DirEntry::eDirectory:
			if (recurseSubdirs) {
				dirsec = AddDirectory(e->fName, entryNum);
				if (dirsec == 0) {
					ok = false;
				} else {
					char newpath[PATH_MAX];
					snprintf(newpath, PATH_MAX-1, "%s%c%s", fDirectory, DIR_SEPARATOR, e->fName);
					newpath[PATH_MAX-1] = 0;
					fSubdir[entryNum] = new Dos2xUtils(fImage, newpath, fObserver, dirsec,
						fDosFormat, fIsDos25EnhancedDensity, fUse16BitSectorLinks,
						fNumberOfVTOCs);
					if (!fSubdir[entryNum]->AddFiles(piconametype)) {
						ok = false;
					}
				}
			} else {
			}
			break;
		case DirEntry::eParentDirectory:
			break;
		case DirEntry::eLink:
			//DPRINTF("skipping link \"%s\"", e->fName);
			break;
		default:
			DPRINTF("skipping \"%s\" - invalid type %d", e->fName, e->fType);
			break;
		}
	}
	if ((fEntryCount == eMaxEntries) && (i<num)) {
		AERROR("more than %d entries in \"%s\", skipping files", eMaxEntries, fDirectory);
		ok = false;
	}
	if (piconametype != eNoPicoName) {
	       	if (fEntryCount < eMaxEntries) {
			if (!CreatePiconame(piconametype)) {
				AERROR("adding PICONAME.TXT failed!");
				ok = false;
			}
		} else {
			AERROR("cannot add PICONAME.TXT, directory full!");
			ok = false;
		}
	}
	return ok;
}

bool Dos2xUtils::AddFile(const char* name)
{
	char p[PATH_MAX];

	if (fDirectory) {
		if (fIsRootDirectory) {
			snprintf(p, PATH_MAX-1, "%s%s", fDirectory, name);
		} else {
			snprintf(p, PATH_MAX-1, "%s%c%s", fDirectory, DIR_SEPARATOR, name);
		}
	} else {
		if (realpath(name, p) == 0) {
			AERROR("realpath(\"%s\") failed",name);
			return false;
		}
	}
	p[PATH_MAX-1] = 0;
	char* fn = strrchr(p,DIR_SEPARATOR);
	if (!fn) {
		AssertMsg(false, "file-path error in AddFile");
		return false;
	}
	fn++;

	uint8_t * buffer = 0;

	unsigned int seclen = fImage->GetSectorLength();
	unsigned int fileSeclen;
	long filelen;
	bool sector720 = false;

	FILE* f;
	if ((f=fopen(p, "rb")) == NULL) {
		AERROR("cannot open file \"%s\"", p);
		return false;
	}
	if(fseek(f, 0, SEEK_END)) {
		AERROR("cannot seek to end of \"%s\"", p);
		fclose(f);
		return false;
	}
	filelen = ftell(f);
	if (filelen > MAX_FILE_LEN) {
		filelen = MAX_FILE_LEN;
		AWARN("\"%s\" is too large, truncating to %ld bytes", p, filelen);
	}
	if (fseek(f, 0, SEEK_SET)) {
		AERROR("cannot seek to start of \"%s\"", p);
		fclose(f);
		return false;
	}

	if (filelen > 0) {
		buffer = new uint8_t[filelen];
		if (fread(buffer, 1, filelen, f) != (size_t) filelen) {
			AERROR("reading file \"%s\" failed", p);
			fclose(f);
			delete[] buffer;
			return false;
		}
	}
	fclose(f);

	fileSeclen = ( filelen + seclen - 4) / (seclen - 3);

	if (fileSeclen == 0) {
		fileSeclen = 1;
	}

	if ( fileSeclen > GetNumberOfFreeSectors() ) {
		AERROR("not enough space for file \"%s\" (need: %d free: %d)", p,
			fileSeclen, GetNumberOfFreeSectors());
		if (buffer) {
			delete[] buffer;
		}
		return false;
	}

	char* atariname;
	unsigned int entryNum;

	if (!AddEntry(fn, atariname, entryNum)) {
		if (buffer) {
			delete[] buffer;
		}
		return false;
	}

	unsigned int* sectors = new unsigned int[fileSeclen];

	//DPRINTF("file needs %d sectors", fileSeclen);
	if (!AllocSectors(fileSeclen, sectors, false)) {
		delete[] sectors;
		DPRINTF("allocating %d sectors failed", fileSeclen);
		if (buffer) {
			delete[] buffer;
		}
		return false;
	}


	WriteBufferToImage(entryNum, buffer, filelen, sectors, fileSeclen, sector720);

	unsigned int startsec = sectors[0];
	SetAtariDirectory(entryNum, atariname, sector720, false, startsec, fileSeclen);
	ALOG("Added file \"%s\"", p);

	delete[] sectors;
	if (buffer) {
		delete[] buffer;
	}
	return true;
}

unsigned int Dos2xUtils::AddDirectory(const char* name, unsigned int& entryNum)
{
	char p[PATH_MAX];
	snprintf(p, PATH_MAX-1, "%s%c%s", fDirectory, DIR_SEPARATOR, name);
	p[PATH_MAX-1] = 0;

	unsigned int seclen = fImage->GetSectorLength();

	//DPRINTF("add directory \"%s\"", p);

	if ( GetNumberOfFreeSectors() < 8) {
		AWARN("not enough space for directory \"%s\" (need: 8 free: %d)", p,
			GetNumberOfFreeSectors());
		return 0;
	}

	char* atariname;

	if (!AddEntry(name, atariname, entryNum)) {
		return 0;
	}

	unsigned int sectors[8];

	if (!AllocSectors(8, sectors, true)) {
		//DPRINTF("allocating 8 directory sectors failed");
		return 0;
	}


	int i;
	uint8_t buf[256];
	memset(buf, 0, seclen);
	for (i=0;i<8;i++) {
		fImage->WriteSector(sectors[i], buf, seclen);
	}

	SetAtariDirectory(entryNum, atariname, false, true, sectors[0], 8);

	return sectors[0];
}

bool Dos2xUtils::AddDataBlock(const char* atariname, const uint8_t* buffer, unsigned int blocklen)
{
	unsigned int fileSeclen;
	bool sector720 = false;
	unsigned int seclen = fImage->GetSectorLength();

	fileSeclen = ( blocklen + seclen - 4) / (seclen - 3);

	if (fileSeclen == 0) {
		fileSeclen = 1;
	}
	if ( fileSeclen > GetNumberOfFreeSectors() ) {
		AERROR("not enough space for \"%s\" (need: %d free: %d)", atariname,
			fileSeclen, GetNumberOfFreeSectors());
		return false;
	}

	unsigned int entryNum;

	if (!AddEntry(atariname, false, entryNum)) {
		return false;
	}

	unsigned int* sectors = new unsigned int[fileSeclen];

	//DPRINTF("block needs %d sectors", fileSeclen);
	if (!AllocSectors(fileSeclen, sectors, false)) {
		delete[] sectors;
		DPRINTF("allocating %d sectors failed", fileSeclen);
		return false;
	}


	WriteBufferToImage(entryNum, buffer, blocklen, sectors, fileSeclen, sector720);

	unsigned int startsec = sectors[0];
	SetAtariDirectory(entryNum, atariname, sector720, false, startsec, fileSeclen);

	delete[] sectors;
	return true;
}

bool Dos2xUtils::WriteBufferToImage(
	unsigned int entryNum,
	const uint8_t* buffer, unsigned int buffer_size,
	const unsigned int* sectors, unsigned int num_sectors,
	bool& sector720)
{
	bool use16BitLinks = Use16BitSectorLinks();
	unsigned int seclen = fImage->GetSectorLength();
	unsigned int remain = buffer_size;
	unsigned int bytes;
	unsigned int next;
	sector720 = false;
	unsigned int i;
	uint8_t buf[256];
	for (i=0;i<num_sectors;i++) {
		memset(buf, 0, seclen);
		if (remain > seclen - 3) {
			bytes = seclen - 3;
		} else {
			bytes = remain;
		}
		remain -= (seclen - 3);

		memcpy(buf, buffer+i*(seclen-3), bytes);

		buf[seclen-1] = bytes;
		if (i+1 < num_sectors) {
			next = sectors[i+1];
		} else {
			next = 0;
		}
		if (!use16BitLinks) {
			next &= 1023;
			next |= entryNum << 10;
		}
		buf[seclen-2] = next & 0xff;
		buf[seclen-3] = next >> 8;
		fImage->WriteSector(sectors[i], buf, seclen);
		if (sectors[i] >= 720) {
			sector720 = true;
		}
	}
	return true;
}

bool Dos2xUtils::SetAtariDirectory(
	unsigned int entryNum,
	const char* atariname,
	bool usedSectorAbove720,
	bool isDirectory,
	unsigned int startSector,
	unsigned int sectorCount)
{
	unsigned int seclen = fImage->GetSectorLength();
	uint8_t buf[256];
	unsigned int dirsec = fDirSector + (entryNum >> 3);
	fImage->ReadSector(dirsec, buf, seclen);

	unsigned int offset = (entryNum & 7) << 4;
	if (isDirectory) {
		buf[offset] = 0x10;
	} else {
		if (usedSectorAbove720 && IsDos25EnhancedDensity()) {
			buf[offset] = 0x03;
		} else {
			buf[offset] = 0x42;
		}
		if (Use16BitSectorLinks()) {
			buf[offset] |= 4;
		}
	}

	buf[offset + 1] = sectorCount & 0xff;
	buf[offset + 2] = sectorCount >> 8;
	buf[offset + 3] = startSector & 0xff;
	buf[offset + 4] = startSector >> 8;
	memcpy(buf+offset+5, atariname, 11);
	fImage->WriteSector(dirsec, buf, seclen);
	return true;
}


bool Dos2xUtils::AddEntry(const char* longname, char*& atariname, unsigned int& entryNumber)
{
	if (fEntryCount == eMaxEntries) {
		AERROR("directory full - skipping %s", longname);
		return false;
	}
	char localatariname[12];
	BuildAtariName(longname, localatariname);
	if (!AddEntry(localatariname, true, entryNumber)) {
		return false;
	}
	atariname = fAtariName[entryNumber];

	fOrigName[entryNumber] = new char[strlen(longname) + 1];
	strcpy (fOrigName[entryNumber], longname);

	return true;
}

bool Dos2xUtils::AddEntry(const char* atariname, bool allowNameChange, unsigned int& entryNumber)
{
	if (fEntryCount == eMaxEntries) {
		AERROR("directory full - skipping %s", atariname);
		return false;
	}
	entryNumber = fEntryCount;

	fAtariName[entryNumber] = new char[12];
	memcpy(fAtariName[entryNumber], atariname, 12);

	if (allowNameChange) {

		// find first blank in filename, otherwise replace last two characters
		const char* ablank = strchr(atariname, ' ');
		int pos = 6;
		if (ablank) {
			pos = ablank - atariname;
		}
		if (pos > 6) {
			pos = 6;
		}

		int i = 0;
		while (!CheckNameUnique(entryNumber)) {
			fAtariName[entryNumber][pos] = '0' + (i / 10);
			fAtariName[entryNumber][pos+1] = '0' + (i % 10);
			i++;
		}
	}
	if (!CheckNameUnique(entryNumber)) {
		delete[] fAtariName[entryNumber];
		fAtariName[entryNumber] = 0;
		return false;
	}

	fEntryCount++;
	return true;
}

bool Dos2xUtils::IsLegalAtariCharacter(char c, bool firstChar) const
{
	if (c >= 'A' && c <= 'Z') {
		return true;
	}

	if (fDosFormat == eMyDos) {
		// MyDos allows underscore but not digit at start
		if (c >= '0' && c <= '9' && !firstChar) {
			return true;
		}
		if (c == '_') {
			return true;
		}
	} else {
		// DOS 2.x allows digits everywhere
		if (c >= '0' && c <='9') {
			return true;
		}
	}
	return false;
}

void Dos2xUtils::BuildAtariName(const char* name, char* atariname)
{
	memset(atariname, ' ', 11);
	atariname[11] = 0;

	int len = strlen(name);
	int ext = len - 1;
	while (ext > 0 && name[ext] != '.') ext--;

	int p = 0;
	int i = ext;
	char c;
	if (ext > 0) {
		i++;
		while ( i < len && p < 3) {
			c = toupper(name[i]);
			if (IsLegalAtariCharacter(c, false)) {
				atariname[8+p++] = c;
			}
			i++;
		}
		len = ext;
	}
	p = 0;
	i = 0;
	while ( i < len && p < 8) {
		c = toupper(name[i]);
		if (IsLegalAtariCharacter(c, (p==0))) {
			atariname[p++] = c;
		}
		i++;
	}
	if (p == 0) {
		memcpy(atariname, "NONAME", 6);
	}
	//DPRINTF("short name of \"%s\" is \"%s\"", name, atariname);
}

bool Dos2xUtils::CheckNameUnique(unsigned int entryNum)
{
	unsigned int i;
	for (i=0;i<entryNum;i++) {
		if (strcmp(fAtariName[i], fAtariName[entryNum]) == 0) {
			return false;
		}
	}
	return true;
}

bool Dos2xUtils::CreatePiconame(EPicoNameType piconametype)
{
	const int maxlen = 4096;
	const uint8_t EOL = 155;
	uint8_t buf[maxlen];
	char* shortstring;
	unsigned int pos = 0;
	unsigned int e;
	int len, shtlen;
	bool isdir;
	char* name;

	Assert(piconametype != eNoPicoName);

	if (fDirectory) {
		if (fIsRootDirectory) {
			name = fDirectory;
		} else {
			name = strrchr(fDirectory, DIR_SEPARATOR);
			if (name == 0) {
				name = fDirectory;
			} else {
				name++;
			}
		}
		shortstring = ShortenFilename(name, 38, (piconametype == ePicoNameWithoutExtension));
		if (shortstring != 0) {
			len = strlen(shortstring);
			memcpy(buf+pos, shortstring, len);
			pos += len;
			delete[] shortstring;
		}
	}
	buf[pos++] = EOL;

	for (e=0;e<fEntryCount;e++) {
		if (fOrigName[e] && fAtariName[e]) {
			if (fSubdir[e].IsNull()) {
				shtlen = 38;
				isdir = false;
			} else {
				shtlen = 36;
				isdir = true;
			}
			name = strrchr(fOrigName[e], DIR_SEPARATOR);
			if (name == 0) {
				name = fOrigName[e];
			} else {
				name++;
			}
			shortstring = ShortenFilename(name, shtlen, (piconametype == ePicoNameWithoutExtension));
			if (shortstring) {
				len = strlen(shortstring);
				if (len + 15 < maxlen) {
					memcpy(buf+pos, fAtariName[e], 11);
					pos+=11;
					buf[pos++] = ' ';
					if (isdir) {
						buf[pos++] = '>';
						buf[pos++] = ' ';
					}
					memcpy(buf+pos, shortstring, len);
					pos+=len;
					buf[pos++] = EOL;
				}
				delete[] shortstring;
			}
		}
	}
	return AddDataBlock("PICONAMETXT", buf, pos);
}

bool Dos2xUtils::WriteAtariFileToDisk(
	const char* filename,
	unsigned int starting_sector,
	bool use16BitSectorLinks)
{
	unsigned int seclen = fImage->GetSectorLength();
	unsigned int numsec = fImage->GetNumberOfSectors();

	if (starting_sector < 4 || starting_sector > numsec) {
		AERROR("WriteAtariFileToDisk: illegal starting sector");
		return false;
	}

	FILE *f;
	if (!(f=fopen(filename,"wb"))) {
		AERROR("unable to create file \"%s\"", filename);
		return false;
	}
	unsigned int mapsize = (numsec+8) >> 3;
	uint8_t* visited = new uint8_t[mapsize];
	memset(visited, 0, mapsize);

	unsigned int sector = starting_sector;
	unsigned int bytes;
	uint8_t buf[256];

	while (1) {
		if (visited[sector>>3] & (1<<(sector & 7))) {
			AERROR("detected loop in file \"%s\"", filename);
			goto fail;
		}
		visited[sector>>3] |= 1<<(sector & 7);
		if (!fImage->ReadSector(sector, buf, seclen)) {
			AERROR("unable to read sector %d from image\n", sector);
			goto fail;
		}
		bytes = buf[seclen-1];
		//DPRINTF("sector %d: %d bytes", sector, bytes);
		if (bytes) {
			if (fwrite(buf, 1, bytes, f) != bytes) {
				AERROR("unable to write file \"%s\"", filename);
				goto fail;
			}
		}
		if (use16BitSectorLinks) {
			sector = buf[seclen-2] + (buf[seclen-3] << 8);
		} else {
			sector = buf[seclen-2] + ((buf[seclen-3] & 3)<< 8);
		}
		if (sector == 0) {
			break;
		}
		if (sector < 4 || sector > numsec) {
			AERROR("illegal sector number %d in filechain\n", sector);
			goto fail;
		}
	}
	fclose(f);
	delete[] visited;
	return true;
fail:
	fclose(f);
	delete[] visited;
	return false;
}

void Dos2xUtils::IndicateSectorWrite(unsigned int sector)
{
	if (sector < fDirSector || sector > fDirSector + 7) {
		DPRINTF("invalid notification for sector %d", sector);
		return;
	}
	if (!fObserver) {
		DPRINTF("virtual image observer is NULL");
		return;
	}
	uint8_t* oldBuf = fObserver->fOldSectorBuffer;
	uint8_t* newBuf = fObserver->fNewSectorBuffer;

	int i, base;
	base = (sector - fDirSector) * 8;
	for (i=0;i<8;i++) {
		uint8_t oldstat = oldBuf[i*16];
		uint8_t newstat = newBuf[i*16];
		if (oldstat != newstat) {
			//DPRINTF("status change %d: %02x->%02x", i+base, oldstat, newstat);
			char atariname[12];
			memcpy(atariname, newBuf+i*16+5, 11);
			atariname[11] = 0;
			unsigned int sector = newBuf[i*16+3] + (newBuf[i*16+4]<<8);

			if (newstat == 0x80 || newstat == 0) {
				IndicateDeleteEntry(i+base, atariname);
			} else if ((oldstat & 0x43) == 0x43 && (newstat & 0x42) == 0x42) {
				// DOS 2.x close file
				IndicateCloseFile(i+base, atariname, sector, newstat);
			} else if ((oldstat & 0x43) == 0x43 && (newstat & 0x43) == 0x03) {
				// DOS 2.5 close file for files with sectors > 720
				IndicateCloseFile(i+base, atariname, sector, newstat);
			} else if ((oldstat & 0x10) == 0 && (newstat & 0x10) == 0x10) {
				IndicateCreateDirectory(i+base, atariname, sector);
			} else {
				DPRINTF("unhandeled status change %d: %02x->%02x", i+base, oldstat, newstat);
			}
		}
	}
	i=0;
	return;
}

void Dos2xUtils::IndicateDeleteEntry(unsigned int num, const char* /*atariname*/)
{
	//DPRINTF("IndicateDeleteEntry %d \"%s\"", num, atariname);
	if (num >= eMaxEntries) {
		DPRINTF("IndicateDeleteEntry: invalid entry %d", num);
		return;
	}
	if (fOrigName[num]) {
		delete[] fOrigName[num];
		fOrigName[num] = 0;
	}
	if (fAtariName[num]) {
		delete[] fAtariName[num];
		fAtariName[num] = 0;
	}
	fSubdir[num] = 0;
}

void Dos2xUtils::IndicateCloseFile(unsigned int entryNum, const char* atariname, unsigned int sector, uint8_t fileStat)
{
	//DPRINTF("IndicateCloseFile %d \"%s\" %d", entryNum, atariname, sector);
	const char* origname = GetOrigName(entryNum, atariname);
	if (origname == NULL) {
		DPRINTF("unable to get original name of \"%s\"", atariname);
		return;
	}

	bool use16Bit = (fileStat & 4) == 4;

	if (use16Bit) {
		//DPRINTF("using 16 bit sector links");
	}

	fileStat &= 0x42;
	if (IsDos25EnhancedDensity()) {
		fileStat |= 0x40;
	}
	if (fileStat == 0x42) {
		if (!WriteAtariFileToDisk(origname, sector, use16Bit)) {
			AERROR("error writing virtual file \"%s\"", origname);
		} else {
			ALOG("wrote virtual file \"%s\"", origname);
		}
	} else {
		DPRINTF("skipping closed file with status %02x", fileStat);
	}
	delete[] origname;
}

void Dos2xUtils::IndicateCreateDirectory(unsigned int entryNum, const char* atariname, unsigned int sector)
{
	const char* origname = GetOrigName(entryNum, atariname);
	if (origname == NULL) {
		DPRINTF("unable to get original name of \"%s\"", atariname);
		return;
	}

	struct stat statbuf;
	bool need_mkdir = true;
	if (stat(origname, &statbuf) == 0) {
		if (! S_ISDIR(statbuf.st_mode)) {
			AERROR("cannot create virtual directory \"%s\" - file exists", origname);
			delete[] origname;
			return;
		}
		need_mkdir = false;
		//DPRINTF("directory \"%s\" already exists", origname);
	}
	if (need_mkdir) {
#ifdef WINVER
	       	if (mkdir(origname)) {
#else
	       	if (mkdir(origname, S_IREAD | S_IWRITE | S_IEXEC)) {
#endif
			AERROR("creating virtual directory \"%s\" failed", origname);
			delete[] origname;
			return;
		} else {
			ALOG("new directory at sector %d: \"%s\"", sector, origname);
		}
	}
	fSubdir[entryNum] = 0;
	fSubdir[entryNum] = new Dos2xUtils(fImage, origname, fObserver, sector,
		fDosFormat, fIsDos25EnhancedDensity,
		fUse16BitSectorLinks, fNumberOfVTOCs);
	delete[] origname;
}

char* Dos2xUtils::GetOrigName(unsigned int entryNum, const char* atariname)
{
	Assert(atariname && strlen(atariname) == 11);
	if (strcmp(atariname,"           ") == 0) {
		return 0;
	}
	if (strchr(atariname,DIR_SEPARATOR)) {
		return 0;
	}
	if (fAtariName[entryNum] == 0 || strcmp(fAtariName[entryNum], atariname)) {
		fSubdir[entryNum] = 0;
		if (!fAtariName[entryNum]) {
			fAtariName[entryNum] = new char[12];
		}
		strcpy(fAtariName[entryNum], atariname);
		if (fOrigName[entryNum]) {
			delete[] fOrigName[entryNum];
		}
		fOrigName[entryNum] = new char [14];

		int name_len = 8, ext_len = 3;

		while (name_len && atariname[name_len-1] == ' ') {
			name_len--;
		}
		while (ext_len && atariname[8+ext_len-1] == ' ') {
			ext_len--;
		}
		int i,pos = 0;
		for (i=0;i<name_len;i++) {
			fOrigName[entryNum][pos++] = atariname[i];
		}
		if (ext_len) {
			fOrigName[entryNum][pos++] = '.';
			for (i=0;i<ext_len;i++) {
				fOrigName[entryNum][pos++] = atariname[8+i];
			}
		}
		fOrigName[entryNum][pos] = 0;
	}
	char* name = new char[strlen(fDirectory) + strlen(fOrigName[entryNum]) + 2];
	sprintf(name, "%s%c%s", fDirectory, DIR_SEPARATOR, fOrigName[entryNum]);
	return name;
}

Dos2xUtils::Dos2Dir::Dos2Dir()
	: fFileCount(0), fFreeSectors(0)
{
	for (unsigned int i=0;i<eMaxFiles;i++) {
		fFiles[i] = 0;
		fRawName[i] = 0;
	}
}

Dos2xUtils::Dos2Dir::~Dos2Dir()
{
	for (unsigned int i=0;i<eMaxFiles;i++) {
		if (fFiles[i]) {
			delete[] fFiles[i];
		}
		if (fRawName[i]) {
			delete[] fRawName[i];
		}
	}
}

bool Dos2xUtils::Dos2Dir::FindFile(const char* rawname, uint8_t& entryNum, uint8_t& status, unsigned int& startSec) const
{
	unsigned int i;
	for (i=0;i<fFileCount;i++) {
		if (memcmp(rawname, fRawName[i], 11) == 0) {
			entryNum = fEntryNumber[i];
			status = fFileStatus[i];
			startSec = fStartingSector[i];
			return true;
		}
	}
	return false;
}

RCPtr<Dos2xUtils::Dos2Dir> Dos2xUtils::GetDos2Directory(bool beQuiet, unsigned int dirSector) const
{
	RCPtr<Dos2Dir> dir = new Dos2Dir;

	unsigned int sector = dirSector;
	unsigned int fileno;
	uint8_t secbuf[256];
	unsigned int seclen;
	bool done = false;
	unsigned int i;
	unsigned int entriesPerSector = 8;
	bool isBigImage = false;

	seclen = fImage->GetSectorLength();
	
	if (seclen != 128 && seclen != 256) {
		DPRINTF("illegal sector length %d", seclen);
		return 0;
	}
	if (fImage->GetNumberOfSectors() < dirSector+7) {
		if (!beQuiet) {
			AERROR("cannot read directory - image contains less than 368 sectors");
		}
		return 0;
	}

	if (!fImage->ReadSector(360, secbuf, seclen)) {
		DPRINTF("error reading VTOC sector 360");
		return 0;
	}
	dir->fFreeSectors = secbuf[3] + (secbuf[4] << 8);

	if ( (seclen == 256) && (secbuf[0] >= 3) ) {
		isBigImage = true;
	}

	if (fImage->GetNumberOfSectors() == 1040 && seclen == 128 && secbuf[0] == 2) {
		// probably DOS 2.5 enhanced density disk
		if (fImage->ReadSector(1024, secbuf, seclen)) {
			dir->fFreeSectors += secbuf[122] + (secbuf[123] << 8);
		}
	}

	while (!done) {
		if (!fImage->ReadSector(sector, secbuf, seclen)) {
			DPRINTF("error reading directory sector %d", sector);
			return dir;
		}
		if ( isBigImage && (sector == dirSector) ) {
			if (secbuf[128] != 0) {
				entriesPerSector = 16;
			}
		}
		fileno = 0;
		while (fileno < entriesPerSector) {
			uint8_t stat = secbuf[fileno*16];
			if (stat == 0) {
				done = true;
				break;
			}
			if ( ((stat & 0xc3) == 0x42) || ((stat & 0xc3) == 0x03) || ((stat & 0xdf) == 0x10)) {
				char* str = new char[Dos2Dir::eFileWidth + 1];
				dir->fFiles[dir->fFileCount] = str;
				char* rawstr = new char[12];
				memcpy(rawstr, secbuf+fileno*16+5, 11);
				rawstr[11] = 0;
				dir->fRawName[dir->fFileCount] = rawstr;
				dir->fFileStatus[dir->fFileCount] = stat;
				dir->fStartingSector[dir->fFileCount] = secbuf[fileno*16+3] + (secbuf[fileno*16+4] << 8);
				dir->fSectorLength[dir->fFileCount] = secbuf[fileno*16+1] + (secbuf[fileno*16+2] << 8);

				if (entriesPerSector == 8) {
					dir->fEntryNumber[dir->fFileCount] = (sector - dirSector) * 8 + fileno;
				} else {
					dir->fEntryNumber[dir->fFileCount] = 0;
				}
				dir->fFileCount++;

				if (stat & 0x20) {
					str[0] = '*';
				} else {
					str[0] = ' ';
				}
				if ((stat & 0xc3) == 0x03) {
					str[1] = '<';
				} else if ((stat & 0xdf) == 0x10) {
					str[1] = ':';
				} else {
					str[1] = ' ';
				}
				
				for (i=0;i<11;i++) {
					char tmp = secbuf[fileno*16+5+i] & 0x7f;
					if (tmp < 32 || tmp > 126) {
						tmp='.';
					}
					str[i+2] = tmp;
				}
				if ((stat & 0xc3) == 0x03) {
					str[13] = '>';
				} else {
					str[13] = ' ';
				}
				str[14] = 0;
				unsigned int sec = secbuf[fileno*16+1] + (secbuf[fileno*16+2] << 8);
				if (sec < 10000) {
					snprintf(str+14, Dos2Dir::eFileWidth-13, "%04d  ", sec);
				} else {
					snprintf(str+14, Dos2Dir::eFileWidth-13, "9999+ ");
				}
			}
			fileno++;
		}
		sector++;
		if (sector == dirSector+8) {
			done = true;
		}
	}
	return dir;
}

void Dos2xUtils::DumpRawDirectory(bool beQuiet) const
{
	unsigned int sector = 361;
	unsigned int fileno;
	uint8_t secbuf[256];
	unsigned int seclen;
	bool done = false;
	unsigned int i;
	unsigned int entriesPerSector = 8;
	bool isBigImage = false;
	char str[12];
	unsigned int freesectors;

	seclen = fImage->GetSectorLength();
	
	if (seclen != 128 && seclen != 256) {
		DPRINTF("illegal sector length %d", seclen);
		return;
	}
	if (fImage->GetNumberOfSectors() < 368) {
		if (!beQuiet) {
			AERROR("cannot read directory - image contains less than 368 sectors");
		}
		return;
	}

	if (!fImage->ReadSector(360, secbuf, seclen)) {
		DPRINTF("error reading VTOC sector 360");
		return;
	}

	if ( (seclen == 256) && (secbuf[0] >= 3) ) {
		isBigImage = true;
	}

	freesectors = secbuf[3] + (secbuf[4] << 8);
	if (fImage->GetNumberOfSectors() == 1040 && seclen == 128 && secbuf[0] == 2) {
		// probably DOS 2.5 enhanced density disk
		if (fImage->ReadSector(1024, secbuf, seclen)) {
			freesectors += secbuf[122] + (secbuf[123] << 8);
		}
	}

	while (!done) {
		if (!fImage->ReadSector(sector, secbuf, seclen)) {
			DPRINTF("error reading directory sector %d", sector);
			return;
		}
		if ( isBigImage && (sector == 361) ) {
			if (secbuf[128] != 0) {
				entriesPerSector = 16;
			}
		}
		fileno = 0;
		while (fileno < entriesPerSector) {
			uint8_t stat = secbuf[fileno*16];
			if (stat == 0) {
				done = true;
				break;
			}
			unsigned int sec = secbuf[fileno*16+1] + (secbuf[fileno*16+2] << 8);
			unsigned int startsec = secbuf[fileno*16+3] + (secbuf[fileno*16+4] << 8);
			
			for (i=0;i<11;i++) {
				char tmp = secbuf[fileno*16+5+i] & 0x7f;
				if (tmp < 32 || tmp > 126) {
					tmp='.';
				}
				str[i] = tmp;
			}
			str[11] = 0;

			ALOG("%3d %02x %s %05d %05d", fileno, stat, str, startsec, sec);

			fileno++;
		}
		sector++;
		if (sector == 369) {
			done = true;
		}
	}
	ALOG("%d free sectors", freesectors);
}

bool Dos2xUtils::SetDosFormat(EDosFormat format)
{
	unsigned int seclen = fImage->GetSectorLength();

	if (seclen != 128 && seclen != 256) {
		AERROR("SetDosFormat: only SD and DD images supported");
		return false;
	}
	if (fImage->GetNumberOfSectors() < 720) {
		return false;
	}

	fDosFormat = format;
	if (fImage->GetNumberOfSectors() == 1040 &&
	    fImage->GetSectorLength() == 128 &&
	    format == eDos2x) {
		fIsDos25EnhancedDensity = true;
	} else {
		fIsDos25EnhancedDensity = false;
	}       

	if (fImage->GetNumberOfSectors() >= 1024 &&
	    format == eMyDos) {
		fUse16BitSectorLinks = true;
	} else {
		fUse16BitSectorLinks = false;
	}

	return true;
}


bool Dos2xUtils::InitVTOC()
{
	uint8_t buf[256];
	memset(buf, 0, 256);

	unsigned int numsec = fImage->GetNumberOfSectors();
	unsigned int seclen = fImage->GetSectorLength();

	if (seclen != 128 && seclen != 256) {
		AERROR("InitVTOC: only SD and DD images supported");
		return false;
	}

	if (numsec < 720) {
		AERROR("image has less than 720 sectors, aborting VTOC creation");
		return false;
	}
	if (GetDosFormat() == eUnknownDos) {
		return false;
	}

	unsigned int s;
	
	unsigned int lastsec = numsec;
	unsigned int numVTOC = 1;

	fUse16BitSectorLinks=false;

	if (GetDosFormat() == eDos2x) {
		if (IsDos25EnhancedDensity()) {
			lastsec = 1023;

			buf[0] = 2;
			buf[1] = 1010 & 0xff;
			buf[2] = 1010 >> 8;
			buf[3] = 707 & 0xff;
			buf[4] = 707 >> 8;
			fImage->WriteSector(eVTOCSector, buf, seclen);

			memset(buf, 0, seclen);
			buf[122] = 303 & 0xff;
			buf[123] = 303 >> 8;
			fImage->WriteSector(eVTOC2Sector, buf, seclen);
		} else {
			// standard SD/DD disk - limit to 720 sectors
			if (GetDosFormat() == eDos2x) {
				lastsec = 719;
			} else {
				lastsec = 720; // mydos also uses sector 720
			}
			buf[0] = 2;
			buf[1] = buf[3] = (lastsec - 12) & 0xff;
			buf[2] = buf[4] = (lastsec - 12) >> 8;
			fImage->WriteSector(eVTOCSector, buf, seclen);
		}
	} else if (GetDosFormat() == eMyDos) {
		// general MyDos format
		numVTOC = 1 + (numsec + 80) / (seclen * 8);

		if (seclen == e128BytesPerSector) {
			if (numVTOC == 1) {
				buf[0] = 2;
			} else {
				if (numVTOC & 1) {
					// mydos allocates VTOC in pairs of 2 in SD
					numVTOC++;
				}
				buf[0] = (numVTOC+1) / 2 + 2;
				fUse16BitSectorLinks=true;
			}
		} else {
			if (numsec < 1024) {
				buf[0] = 2;
			} else {
				buf[0] = 2 + numVTOC;
				fUse16BitSectorLinks=true;
			}
		}
		buf[1] = buf[3] = (lastsec - 11 - numVTOC) & 0xff;
		buf[2] = buf[4] = (lastsec - 11 - numVTOC) >> 8;

		fImage->WriteSector(eVTOCSector, buf, seclen);

		// clear additional VTOCs
		memset(buf, 0, seclen);
		for (s=1;s<numVTOC;s++) {
			fImage->WriteSector(eVTOCSector - s, buf, seclen);
		}
	} else {
		DPRINTF("illegal DOS format %d", GetDosFormat());
		return false;
	}
	fNumberOfVTOCs = numVTOC;

	MarkSectorsFree(4, eVTOCSector-numVTOC);
	
	if (IsDos25EnhancedDensity()) {
		MarkSectorsFree(eDirSector+8, 719);
		MarkSectorsFree(721, lastsec);
	} else {
		MarkSectorsFree(eDirSector+8, lastsec);
	}

	WriteBootSectors(eBootAtariSIOMyPicoDos);

	/*
	memset(buf, 0, 128);
	buf[0] = 0;
	buf[1] = 1; // 1 sector boot code
	buf[2] = 0; // start address = $0700
	buf[3] = 7;
	buf[4] = 0;
	buf[5] = 7;
	buf[6] = 0x38; // SEC - indicate unsuccessful boot
	buf[7] = 0x60; // RTS
	fImage->WriteSector(1, buf, 128);
	*/

	// clear directory
	memset(buf, 0, seclen);
	for (s=0;s<7;s++) {
		fImage->WriteSector(eDirSector+s, buf, seclen);
	}

	fImage->SetChanged(true);
	return true;
}

unsigned int Dos2xUtils::FindDosBootTableIdx(EBootType type)
{
	unsigned int i = 0;
	while (DosBootTable[i].bootType != Dos2xUtils::eBootNone && DosBootTable[i].bootType != type) {
		i++;
	}
	if (DosBootTable[i].bootType == Dos2xUtils::eBootNone && type != Dos2xUtils::eBootNone) {
		char msg[100];
		snprintf(msg, 100, "unknown BootType %d in FindDosBootTableIdx", type);
		AssertMsg(false, msg);
		i = 0;
	}
	return i;
}

bool Dos2xUtils::WriteBootSectors(EBootType type, bool autorunMyPicoDos)
{
	int s;
	uint8_t buf[384];
	uint8_t dosentry;
	uint8_t status;
	unsigned int startsec;

	DosBootEntry* entry = DosBootTable + FindDosBootTableIdx(type);

	if (entry->bootSectors) {
		memcpy(buf, entry->bootSectors, 384);
	} else {
		memset(buf, 0, 384);
	}
	bool dd = (fImage->GetSectorLength() == 256);

	if (entry->dosSysName) {
		RCPtr<Dos2Dir> dir = GetDos2Directory();
		if (dir->FindFile(entry->dosSysAtariName, dosentry, status, startsec)) {
			buf[entry->dosSysOfsLo] = startsec & 0xff;
			buf[entry->dosSysOfsHi] = startsec >> 8;
		} else {
			AWARN("cannot find %s - using default boot sectors", entry->dosSysName);
			type = eBootDefault;
			entry = DosBootTable + FindDosBootTableIdx(eBootDefault);
			if (entry->bootSectors) {
				memcpy(buf, entry->bootSectors, 384);
			} else {
				memset(buf, 0, 384);
			}
		}
	}

	if (entry->isMyPicoDos) {
		if (dd) {
			buf[0x0b] = 0xfd;
			buf[0x0d] = 0x00;
			buf[0x0e] = 0x01;
		} else {
			buf[0x0b] = 0x7d;
			buf[0x0d] = 0x80;
			buf[0x0e] = 0x00;
		}
		if (fUse16BitSectorLinks) {
			buf[0x0c] = 0xff;
		} else {
			buf[0x0c] = 0x03;
		}
	}

	switch (type) {
	case eBootMyDos4533:
	case eBootMyDos4534:
	case eBootMyDos455Beta4:
		if (dd) {
			buf[0x000e] = 2;
			buf[0x00c4] = 2;
			buf[0x0011] = 0xfd;
		} else {
			buf[0x000e] = 1;
			buf[0x00c4] = 1;
			buf[0x0011] = 0x7d;
		}
		if (fUse16BitSectorLinks) {
			buf[0x0034] = 0xff;
		} else {
			buf[0x0034] = 0x03;
		}
		break;

	case eBootXDos243F:
	case eBootXDos243N:
		if (dd) {
			buf[0x0021] = 0xfd;
		} else {
			buf[0x0021] = 0x7d;
		}
		break;

	case eBootMyPicoDos404:
		buf[0x0f]=1;
		if (autorunMyPicoDos) {
			buf[0x10]=1;
		}
		break;
	case eBootMyPicoDos404N:
		buf[0x0f]=0;
		if (autorunMyPicoDos) {
			buf[0x10]=1;
		}
		break;

	case eBootMyPicoDos404R:
		buf[0x0f]=1;
		if (autorunMyPicoDos) {
			buf[0x10]=1;
		}
		break;
	case eBootMyPicoDos404RN:
		buf[0x0f]=0;
		if (autorunMyPicoDos) {
			buf[0x10]=1;
		}
		break;

	case eBootMyPicoDos404B:
		if (autorunMyPicoDos) {
			buf[0x0f]=1;
		}
		break;

	case eBootMyPicoDos405B:
	case eBootMyPicoDos406B:
		if (autorunMyPicoDos) {
			buf[0x0f]=1;
		}
		break;

	case eBootMyPicoDos405:
	case eBootMyPicoDos406:
		buf[0x0f]=0x81;
		if (autorunMyPicoDos) {
			buf[0x10]=1;
		}
		break;
	case eBootMyPicoDos405A:
	case eBootMyPicoDos406A:
		buf[0x0f]=0x01;
		if (autorunMyPicoDos) {
			buf[0x10]=1;
		}
		break;
	case eBootMyPicoDos405N:
	case eBootMyPicoDos406N:
		buf[0x0f]=0;
		if (autorunMyPicoDos) {
			buf[0x10]=1;
		}
		break;

	case eBootMyPicoDos405R:
	case eBootMyPicoDos406R:
		buf[0x0f]=0x81;
		if (autorunMyPicoDos) {
			buf[0x10]=1;
		}
		break;
	case eBootMyPicoDos405RA:
	case eBootMyPicoDos406RA:
		buf[0x0f]=0x01;
		if (autorunMyPicoDos) {
			buf[0x10]=1;
		}
		break;
	case eBootMyPicoDos405RN:
	case eBootMyPicoDos406RN:
		buf[0x0f]=0;
		if (autorunMyPicoDos) {
			buf[0x10]=1;
		}
		break;

	case eBootMyPicoDos405S0:
	case eBootMyPicoDos406S0:
		buf[0x0f]=0x81;
		if (autorunMyPicoDos) {
			buf[0x10]=1;
		}
		buf[0x11]=0;
		break;
	case eBootMyPicoDos405S1:
	case eBootMyPicoDos406S1:
		buf[0x0f]=0x81;
		if (autorunMyPicoDos) {
			buf[0x10]=1;
		}
		buf[0x11]=1;
		break;

	case ePicoBoot405:
	case ePicoBoot406:
		if (dd) {
			buf[0] = 0;
		} else {
			buf[0] = 0x80;
		}
		break;

	default:
		break;
	}

	for (s=1;s<=3;s++) {
		fImage->WriteSector(s, buf+(s-1)*128, 128);
	}
	return true;
}

unsigned int Dos2xUtils::GetBootFileLength(EBootType type)
{
	DosBootEntry* entry = DosBootTable + FindDosBootTableIdx(type);
	return entry->dosSysDataLen;
}

bool Dos2xUtils::AddBootFile(EBootType type)
{
	DosBootEntry* entry = DosBootTable + FindDosBootTableIdx(type);

	if (entry->dosSysAtariName && entry->dosSysData && entry->dosSysDataLen) {
		return AddDataBlock(entry->dosSysAtariName, entry->dosSysData, entry->dosSysDataLen);
	} else {
		return true;
	}
}

void Dos2xUtils::MarkSectorsFree(unsigned int first_sector, unsigned int last_sector)
{
	uint8_t vtocBuf[256];

	unsigned int secBit;
	unsigned int secByte;
	unsigned int seclen = fImage->GetSectorLength();

	unsigned int sector;

	if (IsDos25EnhancedDensity()) {
		uint8_t vtoc2Buf[256];
		fImage->ReadSector(eVTOCSector, vtocBuf, seclen);
		fImage->ReadSector(eVTOC2Sector, vtoc2Buf, seclen);

		for (sector=first_sector; sector<=last_sector; sector++) {
			secBit = 128 >> (sector & 7);
			secByte = sector >> 3;

			if (secByte < 90) {
				vtocBuf[ 10 + secByte] |= secBit;

				if (secByte >= 6) {
					vtoc2Buf[secByte - 6] |= secBit;
				}
			} else {
				vtoc2Buf[secByte - 6] |= secBit;
			}
		}
		fImage->WriteSector(eVTOCSector, vtocBuf, seclen);
		fImage->WriteSector(eVTOC2Sector, vtoc2Buf, seclen);
	} else {
		unsigned int lastVtocSector = 0;
		bool vtocChanged = false;
		for (sector=first_sector; sector<=last_sector; sector++) {
			secBit = 128 >> (sector & 7);
			secByte = sector >> 3;
			unsigned int vtocnum = (secByte + 10) / seclen;
			unsigned int vtocSector = eVTOCSector - vtocnum;

			secByte = (secByte + 10) & (seclen-1);

			if (vtocSector != lastVtocSector) {
				if (vtocChanged) {
					fImage->WriteSector(lastVtocSector, vtocBuf, seclen);
				}
				fImage->ReadSector(vtocSector, vtocBuf, seclen);
				lastVtocSector = vtocSector;
				vtocChanged = false;
			}

			vtocBuf[secByte] |= secBit;
			vtocChanged = true;
		}
		if (vtocChanged) {
			fImage->WriteSector(lastVtocSector, vtocBuf, seclen);
		}
	}
}

unsigned int Dos2xUtils::GetNumberOfFreeSectors()
{
	if (GetDosFormat() == eUnknownDos) {
		return 0;
	}

	unsigned int seclen = fImage->GetSectorLength();
	uint8_t buf[seclen];

	fImage->ReadSector(eVTOCSector, buf, seclen);
	unsigned int freesec = buf[3] + (buf[4] << 8);

	if (IsDos25EnhancedDensity()) {
		fImage->ReadSector(eVTOC2Sector, buf, seclen);
		freesec += buf[122] + (buf[123] << 8);
	}

	return freesec;
}

bool  Dos2xUtils::AllocSectors(unsigned int num, unsigned int * secnums, bool allocdir)
{
	if (num > GetNumberOfFreeSectors()) {
		return false;
	}
	//DPRINTF("alloc: need %d (free: %d)", num, GetNumberOfFreeSectors());

	if (allocdir && num != 8) {
		return false;
	}
	if (allocdir && GetDosFormat() != eMyDos) {
		return false;
	}
	unsigned int vtoc = 0;
	unsigned int s, b;
	unsigned int maxvtoc = GetNumberOfVTOCs();
	unsigned int seclen = fImage->GetSectorLength();
	unsigned int sector_base = 0;
	unsigned int count = 0;
	int base = 0;

	uint8_t vtocBuf[256];

	if (IsDos25EnhancedDensity()) {
		uint8_t vtoc2Buf[256];

		fImage->ReadSector(eVTOCSector, vtocBuf, seclen);
		fImage->ReadSector(eVTOC2Sector, vtoc2Buf, seclen);

		base = 10;
		b = base;
		unsigned int below_720 = 0;
		unsigned int above_720 = 0;

		while (num && b < 100) {
			if (vtocBuf[b]) {
				s = 0;
				while (num && s < 8) {
					if ( vtocBuf[b] & (128 >> s) ) {
						vtocBuf[b] &= ~(128 >> s);
						if ((int)b >= base + 6) {
							vtoc2Buf[b-base-6] &= ~(128 >> s);
						}
						secnums[count++] = sector_base + 8*(b-base) + s;
						//DPRINTF("got low sector %d", secnums[count-1]);
						below_720++;
						num--;
					}
					s++;
				}
			}
			b++;
		}
		base = - 6;
		b = 84;
		while (num && b < 122) {
			if (vtoc2Buf[b]) {
				s = 0;
				while (num && s < 8) {
					if ( vtoc2Buf[b] & (128 >> s) ) {
						vtoc2Buf[b] &= ~(128 >> s);
						secnums[count++] = sector_base + 8*(b-base) + s;
						//DPRINTF("got high sector %d", secnums[count-1]);
						above_720++;
						num--;
					}
					s++;
				}
			}
			b++;
		}
		unsigned int freesec = vtocBuf[3] + (vtocBuf[4] << 8);
		freesec -= below_720;
		vtocBuf[3] = freesec & 0xff;
		vtocBuf[4] = freesec >> 8;

		freesec = vtoc2Buf[122] + (vtoc2Buf[123] << 8);
		freesec -= above_720;
		vtoc2Buf[122] = freesec & 0xff;
		vtoc2Buf[123] = freesec >> 8;
		fImage->WriteSector(eVTOCSector, vtocBuf, seclen);
		fImage->WriteSector(eVTOC2Sector, vtoc2Buf, seclen);
	} else {
		while (num && vtoc < maxvtoc) {
			bool vtocChanged = false;
			fImage->ReadSector(eVTOCSector - vtoc, vtocBuf, seclen);
			if (vtoc == 0) {
				base = 10;
				if (allocdir) {
					b = base + 46; // subdirs start at sector 368
				} else {
					b = base;
				}
				sector_base = 0;
			} else {
				base = 0;
				b = base;
				sector_base = vtoc * seclen * 8 - 80;
			}
			while (num && b < seclen) {
				if (vtocBuf[b]) {
					if (allocdir) {
						if (vtocBuf[b] == 0xff) {
							vtocChanged = true;
							vtocBuf[b] = 0;
							for (s=0;s<8;s++) {
								secnums[count++] = sector_base + 8*(b-base) + s;
							}
							num = 0;
						}
					} else {
						s = 0;
						while (num && s < 8) {
							if ( vtocBuf[b] & (128 >> s) ) {
								vtocChanged = true;
								vtocBuf[b] &= ~(128 >> s);
								secnums[count++] = sector_base + 8*(b-base) + s;
								num--;
							}
							s++;
						}
					}
				}
				b++;
			}
			if (vtocChanged) {
				fImage->WriteSector(eVTOCSector - vtoc, vtocBuf, seclen);
			}
			vtoc++;
		}
		fImage->ReadSector(eVTOCSector, vtocBuf, seclen);
		unsigned int freesec = vtocBuf[3] + (vtocBuf[4] << 8);
		freesec -= count;
		vtocBuf[3] = freesec & 0xff;
		vtocBuf[4] = freesec >> 8;
		fImage->WriteSector(eVTOCSector, vtocBuf, seclen);
	}
	return (num == 0);
}

static void EstimateDirectorySize(const char* in_directory, ESectorLength seclen, Dos2xUtils::EPicoNameType piconametype,
	bool reserveBootEntry, unsigned int& numSectors)
{
	numSectors += 8;

	int maxEntries = 64;
	unsigned int filesize;
	unsigned int picosize = 0;
	char fullpath[PATH_MAX];
	char directory[PATH_MAX];
	char* shortstring;

	strncpy(directory, in_directory, PATH_MAX);
	directory[PATH_MAX-1] = 0;

	size_t dir_len = strlen(directory);
	if (dir_len) {
		if (directory[dir_len-1] == DIR_SEPARATOR) {
			directory[dir_len-1] = 0;
		}
	}

	struct stat statbuf;
	if (stat(directory, &statbuf) != 0) {
		AERROR("cannot stat \"%s\"", directory);
		return;
	}

	if (S_ISREG(statbuf.st_mode)) {
		// calculate the size of a single-file directory
		RCPtr<FileIO> fileio = new StdFileIO();
		if (!fileio->OpenRead(directory)) {
			AERROR("cannot open \"%s\"", directory);
			return;
		}
		filesize = (fileio->GetFileLength()+seclen-4)/(seclen-3);
		if (filesize == 0) {
			filesize = 1;
		}
		fileio->Close();
		numSectors += filesize;
		if (piconametype != Dos2xUtils::eNoPicoName) {
			// with a single file piconame will never be larger than 125 bytes
			numSectors++;
		}
		return;
	}


	RCPtr<Directory> dir = new Directory();
	int num = dir->ReadDirectory(directory, true);
	int count = 0;
	int i;
	if (reserveBootEntry) {
		maxEntries--;
	}

	if (piconametype != Dos2xUtils::eNoPicoName) {
		if (directory[0] == DIR_SEPARATOR && directory[1] == 0) {
			picosize = 2;
		} else {
			const char* dirname = strrchr(directory, DIR_SEPARATOR);
			if (!dirname) {
				dirname = directory;
			} else {
				dirname++;
			}
			shortstring = ShortenFilename(dirname, 38, (piconametype == Dos2xUtils::ePicoNameWithoutExtension));
			if (shortstring) {
				picosize = strlen(shortstring) + 1;
				delete[] shortstring;
			}
		}
	}

	for (i=0;(count < maxEntries) && (i<num);i++) {
		DirEntry* e = dir->Get(i);
		snprintf(fullpath, PATH_MAX-1, "%s%c%s", directory, DIR_SEPARATOR, e->fName);
		fullpath[PATH_MAX-1] = 0;
		switch (e->fType) {
		case DirEntry::eFile:
			filesize = (e->fByteSize+seclen-4)/(seclen-3);
			if (filesize == 0) {
				filesize = 1;
			}
			numSectors += filesize;
			count++;
			if (piconametype != Dos2xUtils::eNoPicoName) {
				shortstring = ShortenFilename(e->fName, 38, (piconametype == Dos2xUtils::ePicoNameWithoutExtension));
				if (shortstring) {
					picosize += strlen(shortstring) + 13;
					delete[] shortstring;
				}
			}
			break;
		case DirEntry::eDirectory:
			EstimateDirectorySize(fullpath, seclen, piconametype, false, numSectors);
			if (piconametype != Dos2xUtils::eNoPicoName) {
				shortstring = ShortenFilename(e->fName, 36, (piconametype == Dos2xUtils::ePicoNameWithoutExtension));
				if (shortstring) {
					picosize += strlen(shortstring) + 15;
					delete[] shortstring;
				}
			}
			count++;
			break;
		default: break;
		}
	}
	if (piconametype != Dos2xUtils::eNoPicoName) {
		if (count < 64) {
			filesize = (picosize+seclen-4)/(seclen-3);
			numSectors += filesize;
		}
	}
}

static unsigned int GetNumVTOC(unsigned int sectors, ESectorLength seclen)
{
	unsigned int numVTOC = 1 + (sectors + 80) / (seclen * 8);
	if (seclen == e128BytesPerSector) {
		if ((numVTOC > 1) && (numVTOC & 1)) {
			numVTOC++;
		}
	}
	return numVTOC;
}

unsigned int Dos2xUtils::EstimateDiskSize(const char* directory, ESectorLength seclen,
	Dos2xUtils::EPicoNameType piconametype, EBootType bootType,
	bool limitTo65535Sectors)
{
	unsigned int numSectors = 0;
	unsigned int bootFileLen = 0;
	bool reserveBootEntry = false;

	bootFileLen = Dos2xUtils::GetBootFileLength(bootType);
	if (bootFileLen) {
		numSectors += (bootFileLen + seclen - 4) / (seclen - 3);
		reserveBootEntry = true;
	}

	// calculate size of directories/files
	EstimateDirectorySize(directory, seclen, piconametype, reserveBootEntry, numSectors);

	// 3 boot sectors
	numSectors += 3;

	// calculate number of VTOC sectors
	unsigned int numVTOC = GetNumVTOC(numSectors, seclen);
	numVTOC = GetNumVTOC(numSectors+numVTOC, seclen);
	numSectors += numVTOC;

	if (numSectors < 720) {
		numSectors = 720;
	}
	if (numSectors > 65535 && limitTo65535Sectors) {
		numSectors = 65535;
	}
	return numSectors;
}

