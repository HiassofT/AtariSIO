/*
   AtpImage.cpp - implementation of the ATP image format

   Copyright (C) 2003, 2004 Matthias Reichl <hias@horus.com>

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

#include <string.h>
#include "AtpImage.h"
#include "Indent.h"
#include "SIOTracer.h"
#include "AtariDebug.h"

AtpImage::AtpImage(unsigned int numberOfTracks)
	: fNumberOfTracks(numberOfTracks)
{
	AllocData();
}

AtpImage::~AtpImage()
{
	FreeData();
}

void AtpImage::AllocData()
{
	if (fNumberOfTracks) {
		fTracks = new AtpTrack[fNumberOfTracks];
		for (unsigned int i=0;i<fNumberOfTracks;i++) {
			fTracks[i].SetTrackNumber(i);
		}
	} else {
		fTracks = 0;
	}
}

void AtpImage::FreeData()
{
	if (fTracks) {
		delete[] fTracks;
	}
	fTracks = 0;
	fNumberOfTracks = 0;
}

bool AtpImage::SetDensity(Atari1050Model::EDiskDensity dens, uint8_t trackno)
{
	if (trackno < fNumberOfTracks) {
		fTracks[trackno].SetDensity(dens);
		SetChanged(true);
		return true;
	} else {
		return false;
	}
}

bool AtpImage::SetDensity(Atari1050Model::EDiskDensity dens)
{
	for (unsigned int i=0;i<fNumberOfTracks;i++) {
		fTracks[i].SetDensity(dens);
	}
	return true;
}

Atari1050Model::EDiskDensity AtpImage::GetDensity(uint8_t trackno) const
{
	if (trackno < fNumberOfTracks) {
		return fTracks[trackno].GetDensity();
	} else {
		return Atari1050Model::eDensityFM;
	}
}

void AtpImage::SetNumberOfTracks(uint8_t tracks)
{
	FreeData();
	fNumberOfTracks = tracks;
	AllocData();
	SetChanged(true);
}

bool AtpImage::AddSector(uint8_t trackno, const RCPtr<AtpSector>& sector)
{
	if (trackno < fNumberOfTracks) {
		fTracks[trackno].AddSector(sector);
		return true;
	} else {
		return false;
	}
	SetChanged(true);
}

bool AtpImage::GetSector(uint8_t trackno,
		uint8_t sectorID,
		RCPtr<AtpSector>& sector,
		unsigned int current_time) const
{
	if (trackno < fNumberOfTracks) {
		return fTracks[trackno].GetSector(sectorID, sector, current_time);
	} else {
		sector = RCPtr<AtpSector>();
		return false;
	}
}

void AtpImage::Dump(std::ostream& os, unsigned int indentlevel)
{
	using std::endl;
	os << Indent(indentlevel)
	   << "begin AtpImage {"
	   << endl
	;
	os << Indent(indentlevel+1)
	   << "Tracks: " << fNumberOfTracks
	   << endl
	;
	os << Indent(indentlevel+1)
	   << "write protected: "
	;
        if (IsWriteProtected()) {
		os << "yes";
	} else {
		os << "no";
	}
	os << endl;
	for (unsigned int i=0;i<fNumberOfTracks;i++) {
		fTracks[i].Dump(os, indentlevel+1);
	}
	os << Indent(indentlevel)
	   << "} // end AtpImage"
	   << endl
	;
}

RCPtr<ChunkWriter> AtpImage::BuildHeaderChunk() const
{
	RCPtr<ChunkWriter> headerChunk(new ChunkWriter("INFO"));

	headerChunk->AppendDword(fNumberOfTracks);
	
	if (IsWriteProtected()) {
		headerChunk->AppendDword(1);
	} else {
		headerChunk->AppendDword(0);
	}

	headerChunk->CloseChunk();
	return headerChunk;
}

bool AtpImage::InitFromHeaderChunk(RCPtr<ChunkReader> chunk)
{
	if (!chunk) {
		return false;
	}

	if (strcmp(chunk->GetChunkName(),"INFO") == 0) {
		unsigned int tmp;
		if (!chunk->ReadDword(tmp)) {
			return false;
		}
		SetNumberOfTracks(tmp);
		if (!chunk->ReadDword(tmp)) {
			return false;
		}
		if ( (tmp & 1) ==0 ) {
			SetWriteProtect(false);
		} else {
			SetWriteProtect(true);
		}

		return true;

	} else {
		return false;
	}
}

bool AtpImage::WriteImageToFile(const char* filename) const
{
	RCPtr<FileIO> fileio;

#ifdef USE_ZLIB 
	fileio = new GZFileIO();
#else   
	fileio = new StdFileIO();
#endif  

	int len = strlen(filename);

	if (len > 3 && strcasecmp(filename+len-3,".gz") == 0) {
#ifdef USE_ZLIB
		fileio = new GZFileIO();
#else
		AERROR("cannot write comressed file - zlib support disabled at compiletime!");
		return false;
#endif
	} else {
		fileio = new StdFileIO();
	}

	if (!fileio->OpenWrite(filename)) {
		AERROR("cannot create %s", filename);
		return false;
	}

	RCPtr<ChunkWriter> fileChunk(new ChunkWriter("FORM"));

	{
		RCPtr<ChunkWriter> atpChunk(new ChunkWriter("ATP1"));

		// write image information and sector data
		atpChunk->AppendChunk(BuildHeaderChunk());

		for (unsigned int i=0; i<fNumberOfTracks;i++) {
			atpChunk->AppendChunk(fTracks[i].BuildTrackChunk());
		}

		atpChunk->CloseChunk();

		fileChunk->AppendChunk(atpChunk);

		// write CRC32 checksum
		unsigned int checksum = atpChunk->CalculateCRC32();

		RCPtr<ChunkWriter> crcChunk(new ChunkWriter("CRC1"));
		crcChunk->AppendDword(checksum);
		crcChunk->CloseChunk();
	
		fileChunk->AppendChunk(crcChunk);

		// write timing information
		RCPtr<ChunkWriter> timingChunk(new ChunkWriter("TIM1"));
		timingChunk->AppendDword(fNumberOfTracks);

		for (unsigned int i=0; i<fNumberOfTracks;i++) {
			timingChunk->AppendChunk(fTracks[i].BuildTrackTimingChunk());
		}
		timingChunk->CloseChunk();
		fileChunk->AppendChunk(timingChunk);
	}

	fileChunk->CloseChunk();

	if (!fileChunk->WriteToFile(fileio)) {
		AERROR("writing atp image failed!");
		fileio->Close();
		return false;
	}
	fileio->Close();
	SetChanged(false);
	return true;
}

bool AtpImage::ReadImageFromFile(const char* filename, bool beQuiet)
{
	RCPtr<FileIO> fileio;

	FreeData();

#ifdef USE_ZLIB 
	fileio = new GZFileIO();
#else   
	fileio = new StdFileIO();
#endif  

	if (!fileio->OpenRead(filename)) {
		if (!beQuiet) {
			AERROR("cannot open \"%s\" for reading",filename);
		}
		return false;
	}

	RCPtr<ChunkReader> fileChunk(ChunkReader::OpenChunkFile(fileio));
	RCPtr<ChunkReader> formChunk;
	RCPtr<ChunkReader> atpChunk;
	RCPtr<ChunkReader> crcChunk;
	RCPtr<ChunkReader> timingChunk;

	if (!fileChunk) {
		goto error;
	}
	
	do {
		formChunk = fileChunk->OpenChunk();
	} while (formChunk && strcmp(formChunk->GetChunkName(),"FORM"));

	if (!formChunk) {
		if (!beQuiet) {
			AERROR("cannot find FORM chunk in file");
		}
		goto error;
	}
	
	do {
		atpChunk = formChunk->OpenChunk();
	} while (atpChunk && strcmp(atpChunk->GetChunkName(),"ATP1"));

	if (!atpChunk) {
		if (!beQuiet) {
			AERROR("cannot find ATP1 chunk in file");
		}
		goto error;
	}

	{
		RCPtr<ChunkReader> headerChunk;
		
		do {
			headerChunk = atpChunk->OpenChunk();
		} while (headerChunk && strcmp(headerChunk->GetChunkName(),"INFO"));

		if (!headerChunk) {
			if (!beQuiet) {
				AERROR("cannot find INFO chunk in file");
			}
			goto error;
		}

		if (!InitFromHeaderChunk(headerChunk)) {
			if (!beQuiet) {
				AERROR("initialization of AtpImage from INFO chunk failed");
			}
			goto error;
		}

		for (unsigned int i=0; i<fNumberOfTracks; i++) {
			RCPtr<ChunkReader> trackChunk;

			do {
				trackChunk = atpChunk->OpenChunk();
			} while (trackChunk && strcmp(trackChunk->GetChunkName(),"TRAK"));

			if (!trackChunk) {
				if (!beQuiet) {
					AERROR("cannot find TRAK (%u) chunk in file", i);
				}
				goto error;
			}

			if (!fTracks[i].InitFromTRAKChunk(trackChunk, beQuiet)) {
				if (!beQuiet) {
					AERROR("initialization of track %u from TRAK chunk failed",i);
				}
				goto error;
			}
			if (fTracks[i].GetTrackNumber() != i) {
				if (!beQuiet) {
					AERROR("invalid track number: expected %u got %u", i, fTracks[i].GetTrackNumber());
				}
				goto error;
			}
		}
	}

	do {
		crcChunk = formChunk->OpenChunk();
	} while (crcChunk && strcmp(crcChunk->GetChunkName(),"CRC1"));

	if (!crcChunk) {
		if (!beQuiet) {
			AERROR("cannot find CRC1 chunk in file");
		}
		goto error;
	}

	{
		unsigned int file_checksum;
		unsigned int calculated_checksum;

		if (!crcChunk->ReadDword(file_checksum)) {
			goto error;
		}

		calculated_checksum = atpChunk->CalculateCRC32();

		if (file_checksum != calculated_checksum) {
			if (!beQuiet) {
				AERROR("checksum error");
			}
			goto error;
		}
	}

	do {
		timingChunk = formChunk->OpenChunk();
	} while (timingChunk && strcmp(timingChunk->GetChunkName(),"TIM1"));

	if (!timingChunk) {
		if (!beQuiet) {
			AERROR("cannot find TIM1 chunk in file");
		}
		goto error;
	}

	{
		unsigned int timing_nr_of_tracks;

		if (!timingChunk->ReadDword(timing_nr_of_tracks)) {
			goto error;
		}

		if (timing_nr_of_tracks != fNumberOfTracks) {
			if (!beQuiet) {
				AERROR("number of tracks in ATP1 and TIM1 chunk don't match");
			}
			goto error;
		}

		for (unsigned int i=0; i<fNumberOfTracks; i++) {
			RCPtr<ChunkReader> trackTimingChunk;

			do {
				trackTimingChunk = timingChunk->OpenChunk();
			} while (trackTimingChunk && strcmp(trackTimingChunk->GetChunkName(),"TTI1"));

			if (!trackTimingChunk) {
				if (!beQuiet) {
					AERROR("cannot find TTI1 (%d) chunk in file", i);
				}
				goto error;
			}

			if (!fTracks[i].SetTimingInformationFromTTI1Chunk(trackTimingChunk, beQuiet)) {
				if (!beQuiet) {
					AERROR("setting timing information of track %d from TTI1 chunk failed",i);
				}
				goto error;
			}
		}
	}

	fileio->Close();
	SetChanged(false);
	return true;
error:
	fileio->Close();
	return false;

}

bool AtpImage::InitBlankSD()
{
	FreeData();
	fNumberOfTracks = 40;
	AllocData();
	uint8_t buf[128];
	memset(buf,0,128);
	for (unsigned int track=0;track<40;track++) {
		for (unsigned int sector=1;sector<=18;sector++) {
			unsigned int position = Atari1050Model::CalculatePositionOfSDSector(
				track, sector);
			AddSector(track, new AtpSector(sector, 128, buf, position, Atari1050Model::eSDSectorTimeLength));
		}
	}
	SetDensity(Atari1050Model::eDensityFM);
	SetChanged(true);
	return true;
}

bool AtpImage::InitBlankED()
{
	FreeData();
	fNumberOfTracks = 40;
	AllocData();
	uint8_t buf[128];
	memset(buf,0,128);
	for (unsigned int track=0;track<40;track++) {
		for (unsigned int sector=1;sector<=26;sector++) {
			unsigned int position = Atari1050Model::CalculatePositionOfEDSector(
				track, sector);
			AddSector(track, new AtpSector(sector, 128, buf, position, Atari1050Model::eEDSectorTimeLength));
		}
	}
	SetDensity(Atari1050Model::eDensityMFM);
	SetChanged(true);
	return true;
}

// currently only returns 90k or 130k (not the real allocated size),
// depending on the format of the first track
size_t AtpImage::GetImageSize() const
{
	if (fNumberOfTracks > 0 && 
	    fTracks[0].GetDensity() == Atari1050Model::eDensityMFM) {
		return 1040*128;
	} else {
		return 720*128;
	}
}

unsigned int AtpImage::GetNumberOfSectors() const
{
	if (fNumberOfTracks > 0) {
	    if (fTracks[0].GetDensity() == Atari1050Model::eDensityMFM) {
			return 1040;
		} else {
			return 720;
		}
	} else {
		return 0;
	}
}

bool AtpImage::IsAtpImage() const
{
	return true;
}

bool AtpImage::ReadSector(unsigned int sector, uint8_t* buffer, unsigned int buffer_length) const
{
	if (sector < 1) {
		return false;
	}

	if (buffer_length != 128) {
		return false;
	}

	RCPtr<AtpSector> sec;

	switch (GetDensity()) {
	case Atari1050Model::eDensityFM:
		if (sector > 720) {
			return false;
		}
		if (!GetSector( (sector - 1) / 18, ( ( sector - 1) % 18 ) + 1, sec)) {
			return false;
		}
		break;
	case Atari1050Model::eDensityMFM:
		if (sector > 1040) {
			return false;
		}
		if (!GetSector( (sector - 1) / 26, ( ( sector - 1) % 26 ) + 1, sec)) {
			return false;
		}
		break;
	default:
		return false;
	}
	if (sec->GetSectorStatus() != 255) {
		return false;
	}
	return sec->GetData(buffer, buffer_length);
}

bool AtpImage::WriteSector(unsigned int sector, const uint8_t* buffer, unsigned int buffer_length)
{
	if (sector < 1) {
		return false;
	}

	if (buffer_length != 128) {
		return false;
	}

	RCPtr<AtpSector> sec;

	switch (GetDensity()) {
	case Atari1050Model::eDensityFM:
		if (sector > 720) {
			return false;
		}
		if (!GetSector( (sector - 1) / 18, ( ( sector - 1) % 18 ) + 1, sec)) {
			return false;
		}
		break;
	case Atari1050Model::eDensityMFM:
		if (sector > 1040) {
			return false;
		}
		if (!GetSector( (sector - 1) / 26, ( ( sector - 1) % 26 ) + 1, sec)) {
			return false;
		}
		break;
	default:
		return false;
	}
	if (sec->GetSectorStatus() != 255) {
		return false;
	}

	return sec->SetData(buffer, buffer_length);
}
