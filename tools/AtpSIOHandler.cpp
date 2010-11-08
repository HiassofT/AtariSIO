/*
   AtpSIOHandler.cpp - handles SIO commands, the image data is stored
   in an AtpImage object

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
#include <stdio.h>

#include "AtpSIOHandler.h"
#include "AtariDebug.h"
#include "MiscUtils.h"
using namespace MiscUtils;

AtpSIOHandler::AtpSIOHandler(const RCPtr<AtpImage>& image)
	: fImage(image),
	  fCurrentDensity(Atari1050Model::eDensityFM),
	  fCurrentTrack(0),
	  fLastFDCStatus(0xff),
	  fLastDiskAccessTimestamp(0)
{
	if (fImage) {
		fCurrentDensity = fImage->GetDensity(0);
	}
	fTracer = SIOTracer::GetInstance();
}

AtpSIOHandler::~AtpSIOHandler()
{
}

int AtpSIOHandler::ProcessCommandFrame(SIO_command_frame& frame, const RCPtr<SIOWrapper>& wrapper)
{
	int ret=0, ret2;

	if (!fImage) {
		DPRINTF("error - no image loaded into AtpSIOHandler!");
		return AbstractSIOHandler::eImageError;
	}

	if (wrapper->GetBaudrate() != 19200) {
		LOG_SIO_MISC("baudrate != 19200 (%d) - ignoring frame",
			wrapper->GetBaudrate());
		return 0;
	}

	MiscUtils::TimestampType currentTime = frame.reception_timestamp;

	fTracer->TraceCommandFrame(frame);

	unsigned int myDriveNo = frame.device_id - 0x30;

	switch (frame.command) {
	case 0x53: {
		/* get status */

		if (wrapper->GetBaudrate() != 19200) {
			fTracer->TraceCommandError(AbstractSIOHandler::eAtpWrongSpeed);
			break;
		}

		if ((ret=wrapper->SendCommandACK())) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_CMD_ACK_FAILED();
			break;
		}

		unsigned int buflen = 4;
		uint8_t buf[buflen];
		const char* description = "[ get status ]";

		if (currentTime < fLastDiskAccessTimestamp +
		     Atari1050Model::eMotorOffDelay
		   ) {
			buf[0] = 0x10;
		} else {
			buf[0] = 0;
		} 
		buf[0] = 0;

		if (fCurrentDensity == Atari1050Model::eDensityMFM) {
			buf[0] |= 0x80; /* enhanced density */
		}
		buf[1] = fLastFDCStatus;
		buf[2] = 0xe0;
		buf[3] = 0;

		if (fImage->IsWriteProtected()) {
			buf[0] |= 0x08;
			/*buf[1] &= ~0x40;*/
		}

		fTracer->TraceCommandOK();
		fTracer->TraceGetStatus(myDriveNo);
		fTracer->TraceDataBlock(buf, buflen, description);

		if ((ret=wrapper->SendComplete())) {
			LOG_SIO_COMPLETE_FAILED();
			break;
		}

		if ((ret=wrapper->SendDataFrame(buf,4))) {
			LOG_SIO_SEND_DATA_FAILED();
			break;
		}

		break;
	}
	case 0x52: {
		/* read sector */
		if (wrapper->GetBaudrate() != 19200) {
			fTracer->TraceCommandError(AbstractSIOHandler::eAtpWrongSpeed);
			break;
		}

		unsigned int sec = frame.aux1 + (frame.aux2<<8);
		unsigned int track, secid;

		if (!SectorToTrackId(sec, track, secid)) {
			if (wrapper->SendCommandNAK()) {
				LOG_SIO_CMD_NAK_FAILED();
			}

			ret = AbstractSIOHandler::eIllegalSectorNumber;

			fTracer->TraceCommandError(ret);
			LOG_SIO_READ_ILLEGAL_SECTOR(sec);

			break;
		}

		if ((ret=wrapper->SendCommandACK())) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_CMD_ACK_FAILED();
			break;
		}

                unsigned int buflen = 128;
		uint8_t buf[buflen];
		const char* description = "[ read sector ]";

		unsigned int delay = 0;

		delay += SpinUpMotor(currentTime);
		delay += SeekToTrack(track);

		RCPtr<AtpSector> sector;

		unsigned int currentPos = (currentTime + delay) % Atari1050Model::eDiskRotationTime;

		if ( fImage->GetDensity(track) == fCurrentDensity &&
		     fImage->GetSector(track, secid, sector, currentPos)) {
			delay +=
				( (sector->GetPosition() + Atari1050Model::eDiskRotationTime - currentPos)
				  % Atari1050Model::eDiskRotationTime)
			;

			// found sector - add time to read the sector from disk
			delay += sector->GetTimeLength();

			fLastFDCStatus = sector->GetSectorStatus();

			if (!sector->GetData(buf, buflen)) {
				DPRINTF("get data from atp sector failed!");
				fLastFDCStatus = 0xef; // record not found
			}

		} else {
			fLastFDCStatus = 0xef; // sector not found
		}

		if (fLastFDCStatus != 0xff) {
			delay += Atari1050Model::eSectorRetryTime;
		}

		if (fLastFDCStatus == 0xff) {
			fTracer->TraceCommandOK();
		} else {
			if (ret == 0) {
				ret = AbstractSIOHandler::eAtpSectorStatus;
			}
			fTracer->TraceCommandError(ret, fLastFDCStatus);
		}

		fTracer->TraceReadSector(myDriveNo, sec);
		fTracer->TraceAtpDelay(delay);
		fTracer->TraceDataBlock(buf, buflen, description);

		WaitUntil(currentTime + delay);

		if (fLastFDCStatus == 0xff) {
			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}
		} else {
			ret = AbstractSIOHandler::eAtpSectorStatus;
			if (wrapper->SendError()) {
				LOG_SIO_ERROR_FAILED();
				break;
			}
		}

		fLastDiskAccessTimestamp = currentTime+delay;

		if ((ret2=wrapper->SendDataFrame(buf, buflen))) {
			LOG_SIO_SEND_DATA_FAILED();
			if (ret==0) ret=ret2;
			break;
		}
		break;
	}
	case 0x50:
	case 0x57: {
		/* write sector */
		if (wrapper->GetBaudrate() != 19200) {
			fTracer->TraceCommandError(AbstractSIOHandler::eAtpWrongSpeed);
			break;
		}

		unsigned int sec = frame.aux1 + (frame.aux2<<8);
		unsigned int track, secid;

		if (!SectorToTrackId(sec, track, secid)) {
			if (wrapper->SendCommandNAK()) {
				LOG_SIO_CMD_NAK_FAILED();
				break;
			}

			ret = AbstractSIOHandler::eIllegalSectorNumber;

			fTracer->TraceCommandError(ret);
			LOG_SIO_WRITE_ILLEGAL_SECTOR(sec);

			break;
		}

		if ((ret=wrapper->SendCommandACK())) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_CMD_ACK_FAILED();
			break;
		}

		unsigned int buflen = 128;
		uint8_t buf[buflen];
		const char* description;

		if (frame.command == 0x50) {
			description = "[ put sector ]";
		} else {
			description = "[ write sector ]";
		}

		if ((ret=wrapper->ReceiveDataFrame(buf, buflen))) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_RECEIVE_DATA_FAILED();
			break;
		}

		bool lastChanged = fImage->Changed();

		unsigned int delay = 0;

		delay += SpinUpMotor(currentTime);
		delay += SeekToTrack(track);

		if (fImage->IsWriteProtected() ) {
			ret = AbstractSIOHandler::eWriteProtected;
			fLastFDCStatus = 0xbf;
		} else {
			RCPtr<AtpSector> sector;

			unsigned int currentPos = (currentTime + delay) % Atari1050Model::eDiskRotationTime;

			if ( fImage->GetDensity(track) == fCurrentDensity &&
			     fImage->GetSector(track, secid, sector, currentPos)) {
				delay +=
					( (sector->GetPosition() + Atari1050Model::eDiskRotationTime - currentPos)
					  % Atari1050Model::eDiskRotationTime)
				;

				// found sector - add time to read the sector from disk
				delay += sector->GetTimeLength();

				fLastFDCStatus = sector->GetSectorStatus();

			} else {
				fLastFDCStatus = 0xef; // sector not found
			}

			if (fLastFDCStatus != 0xff) {
				delay += Atari1050Model::eSectorRetryTime;
			}

			if (fLastFDCStatus == 0xff && frame.command == 0x57) {
				// add verify sector delay
				delay += Atari1050Model::eDiskRotationTime;
			}

			if (sector && !sector->SetData(buf, buflen)) {
				DPRINTF("set data of atp sector failed!");
				fLastFDCStatus = 0xef; // record not found
			}
			fImage->SetChanged(true);
		}

		if (fLastFDCStatus == 0xff) {
			fTracer->TraceCommandOK();
		} else {
			if (ret == 0) {
				ret = AbstractSIOHandler::eAtpSectorStatus;
			}
			fTracer->TraceCommandError(ret, fLastFDCStatus);
		}

		if ((lastChanged == false) && fImage->Changed()) {
			fTracer->IndicateDriveChanged(myDriveNo);
		}
		if (frame.command == 0x50) {
			fTracer->TraceWriteSector(myDriveNo, sec);
		} else {
			fTracer->TraceWriteSectorVerify(myDriveNo, sec);
		}
		fTracer->TraceAtpDelay(delay);
		fTracer->TraceDataBlock(buf, buflen, description);

		WaitUntil(currentTime + delay);

		if (fLastFDCStatus == 0xff) {
			if ((ret2=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				if (ret == 0) ret = ret2;
				break;
			}
		} else {
			if (wrapper->SendError()) {
				LOG_SIO_ERROR_FAILED();
				break;
			}
		}

		fLastDiskAccessTimestamp = currentTime+delay;

		break;
	}

	case 0x21:
	case 0x22: {
		/* format disk */
		if (wrapper->GetBaudrate() != 19200) {
			fTracer->TraceCommandError(AbstractSIOHandler::eAtpWrongSpeed);
			break;
		}

		if ((ret=wrapper->SendCommandACK())) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_CMD_ACK_FAILED();
			break;
		}

		unsigned int buflen = 128;
		uint8_t buf[buflen];
		const char* description;

		if (frame.command == 0x21) {
			fCurrentDensity = Atari1050Model::eDensityFM;
			description = "[ format disk ]";
		} else {
			fCurrentDensity = Atari1050Model::eDensityMFM;
			description = "[ format enhanced density ]";
		}

		bool lastChanged = fImage->Changed();

		unsigned int delay = 0;

		delay += SpinUpMotor(currentTime);
		delay += SeekToTrack(0);

		if (fImage->IsWriteProtected()) {
			ret = AbstractSIOHandler::eWriteProtected;
			fLastFDCStatus = 0xbf;
			memset(buf,0,buflen);
		} else {
			bool r;
			if (frame.command == 0x21) {
				r = fImage->InitBlankSD();
			} else {
				r = fImage->InitBlankED();
			}
			if (!r) {
				DPRINTF("initialization of AtpImage failed");
				ret = AbstractSIOHandler::eImageError;
				fLastFDCStatus = 0xbf;
				memset(buf,0,buflen);
			} else {
				// add delay for formatting disk
				// delay += 40*3*Atari1050Model::eDiskRotationTime;

				fLastFDCStatus = 0xff;
				memset(buf,255,buflen);
			}
		}

		if (fLastFDCStatus == 0xff) {
			fTracer->TraceCommandOK();
		} else {
			if (ret == 0) {
				ret = AbstractSIOHandler::eAtpSectorStatus;
			}
			fTracer->TraceCommandError(ret, fLastFDCStatus);
		}

		if ((lastChanged == false) && fImage->Changed()) {
			fTracer->IndicateDriveChanged(myDriveNo);
		}
		fTracer->IndicateDriveFormatted(myDriveNo);

		if (frame.command == 0x21) {
			fTracer->TraceFormatDisk(myDriveNo);
		} else {
			fTracer->TraceFormatEnhanced(myDriveNo);
		}
		fTracer->TraceAtpDelay(delay);
		fTracer->TraceDataBlock(buf, buflen, description);

		WaitUntil(currentTime + delay);

		if (fLastFDCStatus == 0xff) {
			if ((ret2=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				if (ret==0) ret = ret2;
				break;
			}
		} else {
			if (wrapper->SendError()) {
				LOG_SIO_ERROR_FAILED();
				break;
			}
		}

		if ((ret2=wrapper->SendDataFrame(buf, buflen))) {
			LOG_SIO_SEND_DATA_FAILED();
			if (ret==0) ret=ret2;
			break;
		}

		fLastDiskAccessTimestamp = currentTime+delay;

		break;
	}

	default:
		if (wrapper->SendCommandNAK()) {
			LOG_SIO_CMD_NAK_FAILED();
		}
		ret = AbstractSIOHandler::eUnsupportedCommand;
		fTracer->TraceCommandError(ret);
		break;
	}

	return ret;
}

bool AtpSIOHandler::IsAtpSIOHandler() const
{
	return true;
}

bool AtpSIOHandler::SectorToTrackId(unsigned int secno, unsigned int& trackno, unsigned int& sectorid) const
{
	if (secno==0) {
		return false;
	}
	if (fCurrentDensity == Atari1050Model::eDensityFM) {
		if (secno > 720) {
			return false;
		}
		trackno = (secno-1) / 18;
		sectorid = (secno-1) % 18 + 1;
	} else {
		if (secno > 1040) {
			return false;
		}
		trackno = (secno-1) / 26;
		sectorid = (secno-1) % 26 + 1;
	}
	return true;
}

unsigned int AtpSIOHandler::SeekToTrack(unsigned int track)
{
	unsigned int time = Atari1050Model::CalculateTrackSeekTime(fCurrentTrack, track);
	fCurrentTrack = track;
	return time;
}

unsigned int AtpSIOHandler::SpinUpMotor(const MiscUtils::TimestampType& currentTime)
{
	if (currentTime < fLastDiskAccessTimestamp +
		Atari1050Model::eMotorOffDelay) {
		return 0;
	} else {
		return Atari1050Model::eDiskSpinUpTime;
	}
}

bool AtpSIOHandler::EnableHighSpeed(bool)
{
	return false;
}

bool AtpSIOHandler::EnableXF551Mode(bool)
{
	return false;
}

bool AtpSIOHandler::EnableStrictFormatChecking(bool)
{
	return false;
}

bool AtpSIOHandler::SetHighSpeedParameters(unsigned int, unsigned int)
{
	return false;
}
