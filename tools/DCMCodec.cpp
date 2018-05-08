/*
   DCMCodec.cpp - DCM image format encoder/decoder

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


#include "DCMCodec.h"
#include <stdio.h>
#include <string.h>
#include "SIOTracer.h"
#include "AtariDebug.h"

//#define DDPRINTF(x...) DPRINTF(x)
#define DDPRINTF(x...) do { } while(0)

DCMCodec::DCMCodec(const RCPtr<FileIO>& ioclass, const RCPtr<AtrMemoryImage>& img)
	: fFileIO(ioclass),
	  fAtrMemoryImage(img)
{
}

DCMCodec::~DCMCodec()
{
}

bool DCMCodec::Load( const char* filename, bool beQuiet)
{
	uint8_t	btArcType = 0;		//Block type for first block
	uint8_t	btBlkType;		//Current block type

	fAlreadyFormatted = false;
	fLastPassFlag = false;
	fCurrentSector = 0;

	if (! fFileIO->OpenRead(filename)) {
		if (!beQuiet) {
			AERROR("cannot open \"%s\" for reading",filename);
		}
		return false;
	}

	fFileLength = fFileIO->GetFileLength();

	for(;;) //outpass
	{
		if ( fFileIO->Tell() >= fFileLength )
		{
			if ( ( !fLastPassFlag ) && ( btArcType == eDCM_HEADER_MULTI ) )
			{
				if (!beQuiet) {
					AERROR("DCM multipart archives are unsupported");
				}
				goto failure;
			}
		}

		if (!fFileIO->ReadByte(btArcType)) {
			goto failure_EOF;
		}

		switch( btArcType )
		{
			case eDCM_HEADER_MULTI:
			case eDCM_HEADER_SINGLE:
				if ( !DecodeRecFA(beQuiet) ) {
					if (!beQuiet) {
						DPRINTF("DCM: DecodeRecFA failed");
					}
					goto failure;
				}
				break;

			default:
				if (!beQuiet) {
					DPRINTF("DCM: unknown header block 0x%02x.", btArcType );
				}
				goto failure;
		}

		for(;;) //inpass
		{
			if (!fFileIO->ReadByte(btBlkType)) {
				goto failure_EOF;
			}

			if ( btBlkType == eDCM_PASS_END ) {
				if (!beQuiet) {
					DDPRINTF("pass end");
				}
				break;
			}

			if ( fFileIO->Tell() >= fFileLength )
			{
				goto failure_EOF;
			}

			bool bRes = true;

			switch( btBlkType & 0x7F )
			{
				case eDCM_CHANGE_BEGIN:
					bRes = DecodeRec41(beQuiet);
					break;

				case eDCM_DOS_SECTOR:
					bRes = DecodeRec42(beQuiet);
					break;

				case eDCM_COMPRESSED:
					bRes = DecodeRec43(beQuiet);
					break;

				case eDCM_CHANGE_END:
					bRes = DecodeRec44(beQuiet);
					break;

				case eDCM_SAME_AS_BEFORE:
					//not needed
					//bRes = DecodeRec46(beQuiet);
					break;

				case eDCM_UNCOMPRESSED:
					bRes = DecodeRec47(beQuiet);
					break;

				default:
				{
					switch( btBlkType )
					{
						case eDCM_HEADER_MULTI:
						case eDCM_HEADER_SINGLE:
							if (!beQuiet) {
								DPRINTF("DCM: Trying to start section but last section never had "
								"an end section block.");
							}
							break;

						default:
							if (!beQuiet) {
								DPRINTF("DCM: unknown block type 0x%02x - file may be corrupt.",btBlkType);
							}
							break;
					}

					goto failure;
				}
			}

			if ( !bRes )
			{
				if (!beQuiet) {
					DPRINTF("DCM: Block %02X decode error!", btBlkType );
				}
				goto failure;
			}

			unsigned int len = fAtrMemoryImage->GetSectorLength(fCurrentSector);

			bRes = fAtrMemoryImage->WriteSector(fCurrentSector, fCurrentBuffer, len);

			if (!bRes) {
				if (!beQuiet) {
					DPRINTF("DCM: writing sector %d to memory image failed!", fCurrentSector);
				}
				goto failure;
			}

			if ( btBlkType & 0x80 ) {
				fCurrentSector++;
			} else {
				if (!fFileIO->ReadWord(fCurrentSector)) {
					goto failure_EOF;
				}
			}

		} //infinite for (inpass)

		//End block
		if ( fLastPassFlag ) {
			if (!beQuiet) {
				DDPRINTF("last pass - exit");
			}
			break;
		}

	} //infinite for (outpass)

	fFileIO->Close();
	return true;

failure_EOF:
	if (!beQuiet) {
		AERROR("unexpected EOF in DCM-file");
	}
failure:
	fFileIO->Close();
	return false;

}

bool DCMCodec::DecodeRec41(bool beQuiet)
{
	if (!beQuiet) {
		DDPRINTF("%d: 41", fCurrentSector);
	}
	uint8_t iOffset;
	uint8_t* pbt;
       	if (!fFileIO->ReadByte(iOffset)) {
		goto failure_EOF;
	}
	pbt = fCurrentBuffer + iOffset;

	do
	{
		if (!fFileIO->ReadByte(*pbt)) {
			goto failure_EOF;
		}
		pbt--;
	} while( iOffset-- );

	return true;

failure_EOF:
	if (!beQuiet) {
		AERROR("unexpected EOF in DCM-file (DecodeRec41)");
	}
	return false;
}

bool DCMCodec::DecodeRec42(bool beQuiet)
{
	if (!beQuiet) {
		DDPRINTF("%d: 42", fCurrentSector);
	}
	if (fFileIO->ReadBlock(fCurrentBuffer + 123, 5) != 5) {
		if (!beQuiet) {
			AERROR("unexpected EOF in DCM-file (DecodeRec42)");
		}
		return false;
	}
	memset( fCurrentBuffer, fCurrentBuffer[ 123 ], 123 );
	return true;
}

bool DCMCodec::DecodeRec43(bool beQuiet)
{
	if (!beQuiet) {
		DDPRINTF("%d: 43", fCurrentSector);
	}
	uint8_t* pbtP = fCurrentBuffer;
	uint8_t* pbtE;

	uint8_t* pbtEnd = fCurrentBuffer + fSectorSize;

	unsigned int iTmp;
	uint8_t cTmp;

	do
	{
		//uncompressed string
		if ( pbtP != fCurrentBuffer ) {
			if (!ReadOffset(iTmp, beQuiet) ) {
				goto failure_EOF;
			}
			pbtE = fCurrentBuffer + iTmp;
		}
		else {
			if (! fFileIO->ReadByte(cTmp) ) {
				goto failure_EOF;
			}
			pbtE = fCurrentBuffer + cTmp;
		}

		if ( pbtE < pbtP )
			return false;

		if ( pbtE != pbtP )
		{
			if (fFileIO->ReadBlock(pbtP, pbtE - pbtP) != (unsigned int) (pbtE - pbtP)) {
				goto failure_EOF;
			}
			pbtP = pbtE;
		}

		if ( pbtP >= pbtEnd )
			break;

		//rle compressed string
		if (!ReadOffset(iTmp, beQuiet)) {
			goto failure_EOF;
		}
		pbtE = fCurrentBuffer + iTmp;

		if (!fFileIO->ReadByte(cTmp)) {
			goto failure_EOF;
		}

		if ( pbtE < pbtP )
			return false;

		memset( pbtP, cTmp, pbtE - pbtP );
		pbtP = pbtE;

	} while( pbtP < pbtEnd );

	return true;
failure_EOF:
	if (!beQuiet) {
		AERROR("unexpected EOF in DCM-file (DecodeRec43)");
	}
	return false;
}

bool DCMCodec::DecodeRec44(bool beQuiet)
{
	if (!beQuiet) {
		DDPRINTF("%d: 44", fCurrentSector);
	}
	unsigned int iOffset;
	if (!ReadOffset(iOffset, beQuiet)) {
		goto failure_EOF;
	}

	if (fFileIO->ReadBlock( fCurrentBuffer + iOffset, fSectorSize - iOffset ) != (fSectorSize - iOffset) ) {
		goto failure_EOF;
	}

	return true;
failure_EOF:
	if (!beQuiet) {
		AERROR("unexpected EOF in DCM-file (DecodeRec44)");
	}
	return false;
}

bool DCMCodec::DecodeRec46(bool beQuiet)
{
	if (!beQuiet) {
		DDPRINTF("%d: 46", fCurrentSector);
	}
	return true;
}

bool DCMCodec::DecodeRec47(bool beQuiet)
{
	if (!beQuiet) {
		DDPRINTF("%d: 47", fCurrentSector);
	}
	unsigned int len;
        len = fSectorSize;

	if (fFileIO->ReadBlock(fCurrentBuffer, len) != len) {
		if (!beQuiet) {
			AERROR("unexpected EOF in DCM-file (DecodeRec47)");
		}
		return false;
	}
	return true;
}

bool DCMCodec::DecodeRecFA(bool beQuiet)
{
	uint8_t btPom;
	EDiskFormat dformat = eNoDisk;
	uint8_t btDensity;
       
	if (!fFileIO->ReadByte(btPom)) {
		goto failure_EOF;
	}

	btDensity = ( btPom >> 5 ) & 0x03;
	fLastPassFlag = ( btPom & 0x80 ) ? true : false;

	if (!beQuiet) {
		DDPRINTF("%d: FA pass=%d last=%d", fCurrentSector, btPom & 0x1F, (int)fLastPassFlag);
	}

	switch( btDensity )
	{
		case eDCM_DENSITY_SD:
			dformat = e90kDisk;
			break;

		case eDCM_DENSITY_ED:
			dformat = e130kDisk;
			break;

		case eDCM_DENSITY_DD:
			dformat = e180kDisk;
			break;

		default:
			if (!beQuiet) {
				AERROR("DCM: unknown density type 0x%02x", btDensity );
			}
			return false;
	}

	if ( !fAlreadyFormatted )
	{
		if (!fAtrMemoryImage->CreateImage(dformat)) {
			if (!beQuiet) {
				DPRINTF("DCM: cannot format memory image!");
			}
			return false;
		}
		fSectorSize = fAtrMemoryImage->GetSectorLength();

		fAlreadyFormatted = true;
	}
	
	if (!fFileIO->ReadWord(fCurrentSector)) {
		goto failure_EOF;
	}

	return true;
failure_EOF:
	if (!beQuiet) {
		AERROR("unexpected EOF in DCM-file (DecodeRecFA)");
	}
	return false;

}

bool DCMCodec::ReadOffset( unsigned int& off, bool beQuiet )
{
	uint8_t bt;
	if (!fFileIO->ReadByte(bt)) {
		if (!beQuiet) {
			AERROR("unexpected EOF in DCM-file (ReadOffset)");
		}
		return false;
	}
	if (bt == 0) {
		off=256;
	} else {
		off=bt;
	}
	return true;
}

bool DCMCodec::Save( const char* filename )
{
	int iDensity;
	fSectorSize = fAtrMemoryImage->GetSectorLength();

	switch (fAtrMemoryImage->GetDiskFormat()) {
	case e90kDisk:
		iDensity = eDCM_DENSITY_SD;
		break;
	case e130kDisk:
		iDensity = eDCM_DENSITY_ED;
		break;
	case e180kDisk:
		iDensity = eDCM_DENSITY_DD;
		break;
	default:
		AERROR("DCM: unsupported disk format!");
		return false;
	}

	if (!fFileIO->OpenWrite(filename)) {
		AERROR("cannot open \"%s\" for writing",filename);
		return false;
	}

	int iPass = 1;

	fPassBuffer = new uint8_t [ 0x6500 ];

	unsigned int iFirstSector = 0;
	unsigned int iPrevSector = 0;
	fCurrentSector = 1;
	unsigned int iNumSectors = fAtrMemoryImage->GetNumberOfSectors();

	memset( fPrevBuffer, 0, fSectorSize );
	memset( fCurrentBuffer, 0, fSectorSize );

	EncodeRecFA( false, iPass, iDensity, iFirstSector );

	//here should be other compression

	while( fCurrentSector <= iNumSectors )
	{
		iFirstSector = 0;

		while( ( fCurrentPtr - fPassBuffer ) < 0x5EFD )
		{
			if ( fCurrentSector > iNumSectors )
				break;

			unsigned int secLen = fAtrMemoryImage->GetSectorLength(fCurrentSector);

			if (!fAtrMemoryImage->ReadSector(fCurrentSector, fCurrentBuffer, secLen )) {
				DPRINTF("DCM: reading sector %d of memory image failed!",fCurrentSector);
				goto failure;
			}

			unsigned int i=0;
			while ( (i < secLen) && (fCurrentBuffer[i] == 0) ) {
				i++;
			}

			bool bSkip = (i==secLen);

			//first non empty sector is marked as first, what a surprise! :)
			if ( !bSkip && !iFirstSector )
			{
				iFirstSector = fCurrentSector;
				iPrevSector = fCurrentSector;
			}

			//if just skipped, increment sector
			if ( bSkip )
			{
				fCurrentSector++;
			}
			else
			{
				//if there is a gap, write sector number
				if ( ( fCurrentSector - iPrevSector ) > 1 )
				{
					*( fCurrentPtr++ ) = fCurrentSector;
					*( fCurrentPtr++ ) = fCurrentSector >> 8;
				}
				else
				{
					//else mark previous record
					*fLastRec |= 0x80;
				}

				//first sector could be encoded with only some data
				if ( fCurrentSector == iFirstSector )
					EncodeRec( true );
				else
				{
					//if are same, encode as record 46
					if ( !memcmp( fPrevBuffer, fCurrentBuffer, fSectorSize ) )
						EncodeRec46();
					else
						EncodeRec( false );
				}

				//store this sector as previous
				memcpy( fPrevBuffer, fCurrentBuffer, fSectorSize );

				//and move pointers
				iPrevSector = fCurrentSector;
				fCurrentSector++;
			}

		}

		*fLastRec |= 0x80;

		//encode end
		EncodeRec45();

		uint8_t* pEnd = fCurrentPtr;

		//change beginning block
		if ( fCurrentSector > iNumSectors )
			EncodeRecFA( true, iPass, iDensity, iFirstSector );
		else
			EncodeRecFA( false, iPass, iDensity, iFirstSector );

		//and write whole pass

		if ( ( pEnd - fPassBuffer ) > 0x6000 )
		{
			DPRINTF("DCM: internal error, pass too long!");
			goto failure;
		}

		if ( fFileIO->WriteBlock( fPassBuffer, pEnd - fPassBuffer ) != (size_t) (pEnd - fPassBuffer) )
		{
			AERROR("DCMCodec::Save(): error writing to file \"%s\"",filename);
			goto failure;
		}

		iPass++;
	}

	fFileIO->Close();
	delete [] fPassBuffer;
	return true;

failure:
	fFileIO->Close();
	fFileIO->Unlink(filename);
	delete [] fPassBuffer;
	return false;
}

void DCMCodec::EncodeRecFA( bool bLast, int iPass, int iDensity, int iFirstSec )
{
	DDPRINTF("%d: FA pass=%d last=%d", fCurrentSector, iPass, (int)bLast);
	fCurrentPtr = fPassBuffer;
	
	fLastRec = fCurrentPtr;

	uint8_t btType = bLast ? 0x80 : 0;

	btType |= ( iDensity & 3 ) << 5;

	btType |= ( iPass & 0x1F );

	*( fCurrentPtr++ ) = eDCM_HEADER_SINGLE;
	*( fCurrentPtr++ ) = btType;
	*( fCurrentPtr++ ) = iFirstSec;
	*( fCurrentPtr++ ) = iFirstSec >> 8;

}

void DCMCodec::EncodeRec45()
{
	DDPRINTF("%d: 45", fCurrentSector);

	fLastRec = fCurrentPtr;
	*( fCurrentPtr++ ) = eDCM_PASS_END;
}

void DCMCodec::EncodeRec46()
{
	DDPRINTF("%d: 46", fCurrentSector);
	
	fLastRec = fCurrentPtr;
	*( fCurrentPtr++ ) = eDCM_SAME_AS_BEFORE;
}

void DCMCodec::EncodeRec( bool bIsFirstSector )
{
	fLastRec = fCurrentPtr;

	uint8_t abtBuff41[ 0x300 ];
	uint8_t abtBuff43[ 0x300 ];
	uint8_t abtBuff44[ 0x300 ];
	uint8_t* abtBuff47 = fCurrentBuffer;

	int iEnd41 = 0x300;
	int iEnd43 = 0x300;
	int iEnd44 = 0x300;

	int iBestMethod = eDCM_UNCOMPRESSED;
	int iBestEnd = fSectorSize;
	uint8_t* pbtBest = abtBuff47;

	EncodeRec43( abtBuff43, &iEnd43, fCurrentBuffer, fSectorSize );

	if ( !bIsFirstSector )
	{
		EncodeRec41( abtBuff41, &iEnd41, fCurrentBuffer, fPrevBuffer, fSectorSize );
		EncodeRec44( abtBuff44, &iEnd44, fCurrentBuffer, fPrevBuffer, fSectorSize );
	}

	if ( iEnd41 < 255 && iEnd41 < iBestEnd )
	{
		iBestMethod = eDCM_CHANGE_BEGIN;
		iBestEnd = iEnd41;
		pbtBest = abtBuff41;
	}

	if ( iEnd43 < 255 && iEnd43 < iBestEnd )
	{
		iBestMethod = eDCM_COMPRESSED;
		iBestEnd = iEnd43;
		pbtBest = abtBuff43;
	}

	if ( iEnd44 < 255 && iEnd44 < iBestEnd )
	{
		iBestMethod = eDCM_CHANGE_END;
		iBestEnd = iEnd44;
		pbtBest = abtBuff44;
	}

	DDPRINTF("%d: %2x", fCurrentSector, iBestMethod);

	*( fCurrentPtr++ ) = iBestMethod;
	memcpy( fCurrentPtr, pbtBest, iBestEnd );
	fCurrentPtr += iBestEnd;
}

void DCMCodec::EncodeRec41( uint8_t* pbtDest, int* piDestLen, uint8_t* pbtSrc, uint8_t* pbtSrcOld, int iSrcLen )
{
	uint8_t* pbtS = pbtSrc + iSrcLen - 1;
	pbtSrcOld += iSrcLen - 1;

	uint8_t* pbtD = pbtDest;

	for( int i = 0; i < iSrcLen; i++ )
	{
		if ( *( pbtS-- ) != * ( pbtSrcOld-- ) )
			break;
	}

	pbtS++;

	*( pbtD++ ) = pbtS - pbtSrc;

	int iBytes = pbtS - pbtSrc + 1;

	while( iBytes-- )
	{
		*( pbtD++ ) = *( pbtS-- );
	}

	*piDestLen = pbtD - pbtDest;
}

void DCMCodec::EncodeRec43( uint8_t* pbtDest, int* piDestLen, uint8_t* pbtSrc, int iSrcLen )
{
	uint8_t* pbtEnd = pbtSrc + iSrcLen;
	uint8_t* pbtCur = pbtSrc;

	uint8_t* pbtD = pbtDest;

	while( pbtCur < pbtEnd )
	{
		bool bFound = false;

		for( uint8_t* pbtNow = pbtCur; pbtNow < ( pbtEnd - 2 ); pbtNow++ )
		{

			if ( ( *pbtNow == *(pbtNow+1) ) && ( *pbtNow == *(pbtNow+2) ) )
			{
				int iUnc = pbtNow - pbtCur;
				
				*( pbtD ++ ) = pbtNow - pbtSrc;
				if ( iUnc )
				{
					memcpy( pbtD, pbtCur, iUnc );
					pbtD += iUnc;
				}

				uint8_t bt = *pbtNow;
				uint8_t*p;
				for( p = pbtNow + 1; p < pbtEnd; p++ )
				{
					if ( *p != bt )
						break;
				}

				if ( p > pbtEnd )
					p = pbtEnd;

				*( pbtD++ ) = p - pbtSrc;
				*( pbtD++ ) = bt;

				pbtCur = p;
				bFound = true;
				break;
			}
		}

		if ( ( pbtCur >= pbtEnd - 2 ) || !bFound ) 
		{
			if ( pbtCur < pbtEnd )
			{
				*( pbtD++ ) = iSrcLen;
				memcpy( pbtD, pbtCur, pbtEnd - pbtCur );
				pbtD += pbtEnd - pbtCur;
			}

			break;
		}

	}

	*piDestLen = pbtD - pbtDest;
}

void DCMCodec::EncodeRec44( uint8_t* pbtDest, int* piDestLen, uint8_t* pbtSrc, uint8_t* pbtSrcOld, int iSrcLen )
{
	uint8_t* pbtS = pbtSrc;
	uint8_t* pbtEnd = pbtSrc + iSrcLen;

	uint8_t* pbtD = pbtDest;

	for( int i = 0; i < iSrcLen; i++ )
	{
		if ( *( pbtS++ ) != * ( pbtSrcOld++ ) )
			break;
	}

	pbtS--;

	*( pbtD++ ) = pbtS - pbtSrc;
	memcpy( pbtD, pbtS, pbtEnd - pbtS );
	pbtD += pbtEnd - pbtS;

	*piDestLen = pbtD - pbtDest;
}

