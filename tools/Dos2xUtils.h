#ifndef DOS2XUTILS_H
#define DOS2XUTILS_H

/*
   Dos2xUtils - utilities to access DOS 2.x images

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

#include "RefCounted.h"
#include "RCPtr.h"
#include "AtrImage.h"
#include "VirtualImageObserver.h"

class Dos2xUtils : public RefCounted {
public:
	Dos2xUtils(RCPtr<DiskImage> img, const char* directory = 0,
		RCPtr<VirtualImageObserver> observer = RCPtr<VirtualImageObserver>() );

	virtual ~Dos2xUtils();

	enum EPicoNameType {
		eNoPicoName,
		ePicoName,
		ePicoNameWithoutExtension
	};

	bool AddFiles(EPicoNameType piconametype = eNoPicoName);

	// DOS 2.x file access methods
	enum EDosFormat {
		eUnknownDos = 0,
		eDos2x = 1,
		eMyDos = 2
	};
	enum {
		eVTOCSector = 360,
		eVTOC2Sector = 1024, // for DOS 2.5 enhanced density
		eDirSector = 361
	};

	bool SetDosFormat(EDosFormat format);

	bool InitVTOC();

	enum EBootType {
		eBootNone = 0,

		eBootAtariSIOMyPicoDos,
		eBootDos20,
		eBootDos25,
		eBootMyDos4533,
		eBootMyDos4534,
		eBootMyDos455Beta4,

		eBootTurboDos21,
		eBootTurboDos21HS,

		eBootXDos243F,
		eBootXDos243N,

		eBootMyPicoDos403,
		eBootMyPicoDos403HS,

		eBootMyPicoDos404,
		eBootMyPicoDos404N,
		eBootMyPicoDos404R,
		eBootMyPicoDos404RN,
		eBootMyPicoDos404B,

		eBootMyPicoDos405,
		eBootMyPicoDos405A,
		eBootMyPicoDos405N,
		eBootMyPicoDos405R,
		eBootMyPicoDos405RA,
		eBootMyPicoDos405RN,
		eBootMyPicoDos405B,
		eBootMyPicoDos405S0,
		eBootMyPicoDos405S1,

		ePicoBoot405,

		eBootMyPicoDos406,
		eBootMyPicoDos406A,
		eBootMyPicoDos406N,
		eBootMyPicoDos406R,
		eBootMyPicoDos406RA,
		eBootMyPicoDos406RN,
		eBootMyPicoDos406B,
		eBootMyPicoDos406S0,
		eBootMyPicoDos406S1,

		ePicoBoot406,

		eBootDefault = eBootAtariSIOMyPicoDos
	};

	bool WriteBootSectors(EBootType type, bool autorunMyPicoDos = false);
	static unsigned int GetBootFileLength(EBootType type);
	bool AddBootFile(EBootType type);

	static unsigned int EstimateDiskSize(const char* directory, ESectorLength seclen, 
		EPicoNameType piconametype = eNoPicoName, EBootType bootType = eBootDefault,
		bool limitTo65535Sectors = true);

	unsigned int GetNumberOfFreeSectors();
	bool AllocSectors(unsigned int num, unsigned int * secnums, bool allocdir = false);

	inline EDosFormat GetDosFormat() const { return fDosFormat; }
	inline bool IsDos25EnhancedDensity() const { return fIsDos25EnhancedDensity; }
	inline bool Use16BitSectorLinks() const { return fUse16BitSectorLinks; }
	inline unsigned int GetNumberOfVTOCs() const { return fNumberOfVTOCs; }

	class Dos2Dir : public RefCounted {
	public:
		Dos2Dir();
		~Dos2Dir();

		unsigned int GetNumberOfFiles() const { return fFileCount; }
		const char* GetFile(unsigned int num) const
		{
			if (num < fFileCount) {
				return fFiles[num];
			} else {
				return 0;
			}
		}
		unsigned int GetFreeSectors() const { return fFreeSectors; }
		
		const char* GetRawFilename(unsigned int num) const
		{
			if (num < fFileCount) {
				return fRawName[num];
			} else {
				return 0;
			}
		}

		uint8_t GetFileStatus(unsigned int num) const
		{
			if (num < fFileCount) {
				return fFileStatus[num];
			} else {
				return 0;
			}
		}

		unsigned int GetFileStartingSector(unsigned int num) const
		{
			if (num < fFileCount) {
				return fStartingSector[num];
			} else {
				return 0;
			}
		}

		unsigned int GetFileSectorLength(unsigned int num) const
		{
			if (num < fFileCount) {
				return fSectorLength[num];
			} else {
				return 0;
			}
		}

		unsigned int GetFileEntryNumber(unsigned int num) const
		{
			if (num < fFileCount) {
				return fEntryNumber[num];
			} else {
				return 0;
			}
		}

		enum { eFileWidth = 20 };

		bool FindFile(const char* rawname,
			uint8_t& entryNum, uint8_t& status, unsigned int& startSec) const;

	private:
		friend class Dos2xUtils;

		enum { eMaxFiles = 128 };

		char* fFiles[eMaxFiles];
		char* fRawName[eMaxFiles];
		uint8_t fFileStatus[eMaxFiles];
		uint8_t fEntryNumber[eMaxFiles];
		unsigned int fStartingSector[eMaxFiles];
		unsigned int fSectorLength[eMaxFiles];

		unsigned int fFileCount;
		unsigned int fFreeSectors;
	};

	RCPtr<Dos2Dir> GetDos2Directory(bool beQuiet = false, unsigned int dirSector = 361) const;
	void DumpRawDirectory(bool beQuiet = false) const;

	bool AddFile(const char* filename);
	bool CreatePiconame(EPicoNameType piconametype);

private:
	friend class VirtualImageObserver;
	void IndicateSectorWrite(unsigned int sector);

private:
	Dos2xUtils( DiskImage* img,
		const char* directory,
		VirtualImageObserver* observer,
		unsigned int dirSector,
		EDosFormat format,
		bool isDos25ED,
		bool use16BitLinks,
		unsigned int numVTOC);

	bool AddDataBlock(const char* atariname, const uint8_t* datablock, unsigned int blocklen);
	unsigned int AddDirectory(const char* dirname, unsigned int& entryNum);
	
	bool AddEntry(const char* longname, char*& atariname, unsigned int& entryNumber);
	bool AddEntry(const char* atariname, bool allowNameChange, unsigned int& entryNumber);

	bool WriteBufferToImage(
		unsigned int entrynum,
		const uint8_t* buffer, unsigned int buffer_size,
		const unsigned int* sectors, unsigned int num_sectors,
		bool& usedSectorAbove720);

	bool SetAtariDirectory(
		unsigned int entryNum,
		const char* atariname,
		bool usedSectorAbove720,
		bool isDirectory,
		unsigned int startSector,
		unsigned int sectorCount);

	bool WriteAtariFileToDisk(
		const char* filename,
		unsigned int starting_sector,
		bool use16BitSectorLinks);

	// returns index in DosBootTable
	static unsigned int FindDosBootTableIdx(EBootType type);

	bool IsLegalAtariCharacter(char c, bool firstChar) const;

	void BuildAtariName(const char* name, char* atariname);

	char* GetOrigName(unsigned int entryNum, const char* atariname);


	bool CheckNameUnique(unsigned int entryNum);

	void IndicateDeleteEntry(unsigned int entryNum, const char* atariname);
	void IndicateCloseFile(unsigned int entryNum, const char* atariname, unsigned int sector, uint8_t fileStat);
	void IndicateCreateDirectory(unsigned int entryNum, const char* atariname, unsigned int sector);

	void MarkSectorsFree(unsigned int first_sector, unsigned int last_sector);

	enum { eMaxEntries = 64 };

	unsigned int fDirSector;
	char* fDirectory;

	unsigned int fEntryCount;
	char* fOrigName[eMaxEntries];
	char* fAtariName[eMaxEntries];
	RCPtr<Dos2xUtils> fSubdir[eMaxEntries];

	DiskImage* fImage;
	VirtualImageObserver* fObserver;

	EDosFormat fDosFormat;
	bool fIsDos25EnhancedDensity;
	bool fUse16BitSectorLinks;
	unsigned int fNumberOfVTOCs;

	bool fIsRootDirectory;
};

#endif
