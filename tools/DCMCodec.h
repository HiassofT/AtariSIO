#ifndef DCMCODEC_H
#define DCMCODEC_H

/*
   DCMCodec.h - DCM image format encoder/decoder

   Copyright (c) 2001-2004 Jindrich Kubec, Matthias Reichl

   The original code has been taken from Jindrich Kubec's 'Acvt' program
   and was adapted by Matthias Reichl for use with AtariSIO.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdio.h>

#include "AtrMemoryImage.h"
#include "FileIO.h"

class DCMCodec
{
public:
	DCMCodec(const RCPtr<FileIO>& ioclass, const RCPtr<AtrMemoryImage>& img);
	~DCMCodec();

	bool Load( const char* filename, bool beQuiet);

	bool Save( const char* filename);

private:
	enum {
		eDCM_CHANGE_BEGIN=0x41,
		eDCM_DOS_SECTOR=0x42,
		eDCM_COMPRESSED=0x43,
		eDCM_CHANGE_END=0x44,
		eDCM_PASS_END=0x45,
		eDCM_SAME_AS_BEFORE=0x46,
		eDCM_UNCOMPRESSED=0x47,
		eDCM_HEADER_SINGLE=0xFA,
		eDCM_HEADER_MULTI=0xF9
	};

	enum {
		eDCM_DENSITY_SD=0,
		eDCM_DENSITY_DD=1,
		eDCM_DENSITY_ED=2
 	};

	bool DecodeRec41(bool beQuiet);
	bool DecodeRec42(bool beQuiet);
	bool DecodeRec43(bool beQuiet);
	bool DecodeRec44(bool beQuiet);
	bool DecodeRec46(bool beQuiet);
	bool DecodeRec47(bool beQuiet);
	bool DecodeRecFA(bool beQuiet);

	bool ReadOffset(unsigned int& off, bool beQuiet);

	void EncodeRec41( uint8_t*, int*, uint8_t*, uint8_t*, int );
	void EncodeRec43( uint8_t*, int*, uint8_t*, int );
	void EncodeRec44( uint8_t*, int*, uint8_t*, uint8_t*, int );

	void EncodeRec45();
	void EncodeRec46();
	void EncodeRec( bool );
	void EncodeRecFA( bool, int, int, int );

	bool	fLastPassFlag;
	uint8_t	fCurrentBuffer[ 0x100 ];
	uint8_t	fPrevBuffer[ 0x100 ];
	uint16_t	fSectorSize;
	uint16_t	fCurrentSector;
	off_t 	fFileLength;
	bool	fAlreadyFormatted;

	uint8_t* fCurrentPtr;
	uint8_t* fPassBuffer;
	uint8_t* fLastRec;

	RCPtr<FileIO> fFileIO;
	RCPtr<AtrMemoryImage> fAtrMemoryImage;
};

#endif

