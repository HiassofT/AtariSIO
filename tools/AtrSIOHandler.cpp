/*
   AtrSIOHandler.cpp - handles SIO commands, the image data is stored
   in an AtrImage object

   Copyright (C) 2002-2007 Matthias Reichl <hias@horus.com>

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
#include <time.h>
#include "AtrSIOHandler.h"
#include "AtariDebug.h"
#include "HighSpeedSIOCode.h"
#include "MyPicoDosCode.h"
#include "Version.h"
#include "MiscUtils.h"

/*
 * speed definition bytes for the Atari:
 * the default for ~19200 bit/sec is 40.
 * ~57600 bit/sec uses a value of 8.
 */

// OK
#define SPEED_BYTE_19200 40
#define SPEED_BYTE_38400 16
#define SPEED_BYTE_41890 14
#define SPEED_BYTE_57600 8
#define SPEED_BYTE_61440 7
#define SPEED_BYTE_68266 6
#define SPEED_BYTE_73728 5

// problematic with TurboDOS
#define SPEED_BYTE_80139 4

#define SPEED_BYTE_87771 3

// pure insane :-)
#define SPEED_BYTE_97010 2
#define SPEED_BYTE_108423 1
#define SPEED_BYTE_122880 0


// invalid
#define SPEED_BYTE_70892 5
#define SPEED_BYTE_76800 4

uint8_t AtrSIOHandler::fBuffer[AtrSIOHandler::eBufferSize];

AtrSIOHandler::AtrSIOHandler(const RCPtr<AtrImage>& image)
	: fImage(image),
	  fEnableHighSpeed(false),
	  fEnableXF551Mode(false),
	  fStrictFormatChecking(false),

	  fSpeedByte(SPEED_BYTE_57600),
	  fHighSpeedBaudrate(57600),

//	  fSpeedByte(SPEED_BYTE_108423),
//	  fHighSpeedBaudrate(108423),

//	  fSpeedByte(SPEED_BYTE_97010),
//	  fHighSpeedBaudrate(97010),

//	  fSpeedByte(SPEED_BYTE_87771),
//	  fHighSpeedBaudrate(87771),

	  fLastFDCStatus(0xff)
{
	if (fImage) {
		fImageConfig = fImage->GetImageConfig();
		fFormatConfig = fImageConfig;
	}
	fTracer = SIOTracer::GetInstance();
}

AtrSIOHandler::~AtrSIOHandler()
{
}

int AtrSIOHandler::ProcessCommandFrame(SIO_command_frame& frame, const RCPtr<SIOWrapper>& wrapper)
{
	int ret=0, ret2;
	bool reset_baudrate = false;

	if (!fImage) {
		DPRINTF("error - no image loaded into AtrSIOHandler!");
		return AbstractSIOHandler::eImageError;
	}

	fTracer->TraceCommandFrame(frame);

	uint8_t myDriveNo = frame.device_id - 0x30;
	bool hi_cmd = (frame.command & 0x80) == 0x80;

	switch (frame.command) {
	case 0xd3:
	case 0x53: {
		/* get status */

		if (hi_cmd) {
			if (!fEnableXF551Mode) {
				if (wrapper->SendCommandNAK()) {
					LOG_SIO_CMD_NAK_FAILED();
				}

				ret = AbstractSIOHandler::eUnsupportedCommand;
				fTracer->TraceCommandError(ret);

				break;
			}
			reset_baudrate = true;
			ret = wrapper->SendCommandACKXF551();
		} else {
			ret = wrapper->SendCommandACK();
		}

		if (ret) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_CMD_ACK_FAILED();
			break;
		}

		size_t buflen = 4;

		const char* description = 0;
		switch (frame.command) {
		case 0x53: description = "[ get status ]"; break;
		case 0xd3: description = "[ get status XF551 ]"; break;
		}

		fBuffer[0] = 0x10; // motor on
		if (fFormatConfig.fSectorLength == e128BytesPerSector) {
			if (fFormatConfig.fNumberOfSectors==1040) {
				fBuffer[0] |= 0x80; /* enhanced density */
			}
		} else {
			fBuffer[0] |= 0x20; /* double density */
			if (fFormatConfig.fNumberOfSectors == 1440) {
				fBuffer[0] |= 0x40; /* XF551 QD sets both DD bit and bit 6 (?) */
			}
		}
		fBuffer[1] = fLastFDCStatus;
		if (fEnableXF551Mode) {
			fBuffer[2] = 0xfe;
		} else {
			fBuffer[2] = 0xe0;
		}
		fBuffer[3] = 0;

		if (fImage->IsWriteProtected()) {
			fBuffer[0] |= 0x08;
		}

		fTracer->TraceCommandOK();
		fTracer->TraceGetStatus(myDriveNo, hi_cmd);
		fTracer->TraceDataBlock(fBuffer, buflen, description);

		if ((ret=wrapper->SendComplete())) {
			LOG_SIO_COMPLETE_FAILED();
			break;
		}

		if (hi_cmd) {
			ret = wrapper->SendDataFrameXF551(fBuffer, 4);
			reset_baudrate = false;
		} else {
			ret = wrapper->SendDataFrame(fBuffer, 4);
		}
		if (ret) {
			LOG_SIO_SEND_DATA_FAILED();
			break;
		}

		break;
	}
	case 0xd2:
	case 0x52: {
		/* read sector */
		if (hi_cmd && !fEnableXF551Mode) {
			if (wrapper->SendCommandNAK()) {
				LOG_SIO_CMD_NAK_FAILED();
			}

			ret = AbstractSIOHandler::eUnsupportedCommand;
			fTracer->TraceCommandError(ret);

			break;
		}

		uint16_t sec = frame.aux1 + (frame.aux2<<8);
		if ( sec<=0 || sec > fImageConfig.fNumberOfSectors) {
			if (wrapper->SendCommandNAK()) {
				LOG_SIO_CMD_NAK_FAILED();
			}

			ret = AbstractSIOHandler::eIllegalSectorNumber;

			fTracer->TraceCommandError(ret);
			LOG_SIO_READ_ILLEGAL_SECTOR(sec);

			break;
		}

		if (hi_cmd) {
			reset_baudrate = true;
			ret = wrapper->SendCommandACKXF551();
		} else {
			ret = wrapper->SendCommandACK();
		}
		if (ret) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_CMD_ACK_FAILED();
			break;
		}

		size_t buflen = fImageConfig.GetSectorLength(sec);

		const char* description = 0;
		switch (frame.command) {
		case 0x52: description = "[ read sector ]"; break;
		case 0xd2: description = "[ read sector XF551 ]"; break;
		}

		if (!fImage->ReadSector(sec, fBuffer, buflen)) {
			fLastFDCStatus = 0xef; // record not found;
			ret = AbstractSIOHandler::eImageError;

			fTracer->TraceCommandError(ret);
			fTracer->TraceReadSector(myDriveNo, sec, hi_cmd);
			fTracer->TraceDataBlock(fBuffer, buflen, description);

			if (wrapper->SendError()) {
				LOG_SIO_ERROR_FAILED();
				break;
			}
		} else {
			fLastFDCStatus = 0xff;

			fTracer->TraceCommandOK();
			fTracer->TraceReadSector(myDriveNo, sec, hi_cmd);
			fTracer->TraceDataBlock(fBuffer, buflen, description);

			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}
		}

		if (hi_cmd) {
			ret2 = wrapper->SendDataFrameXF551(fBuffer, buflen);
			reset_baudrate = false;
		} else {
			ret2 = wrapper->SendDataFrame(fBuffer, buflen);
		}
		if (ret2) {
			LOG_SIO_SEND_DATA_FAILED();
			if (ret==0) ret=ret2;
			break;
		}
		break;
	}
	case 0xd0:
	case 0xd7:
	case 0x50:
	case 0x57: {
		/* write sector */
		if (hi_cmd && !fEnableXF551Mode) {
			if (wrapper->SendCommandNAK()) {
				LOG_SIO_CMD_NAK_FAILED();
			}

			ret = AbstractSIOHandler::eUnsupportedCommand;
			fTracer->TraceCommandError(ret);

			break;
		}
		uint16_t sec = frame.aux1 + (frame.aux2<<8);
		if ( sec<=0 || sec > fImageConfig.fNumberOfSectors ) {
			if (wrapper->SendCommandNAK()) {
				LOG_SIO_CMD_NAK_FAILED();
			}
			
			ret = AbstractSIOHandler::eIllegalSectorNumber;

			fTracer->TraceCommandError(ret);
			LOG_SIO_WRITE_ILLEGAL_SECTOR(sec);
			
			break;
		}

		if (hi_cmd) {
			reset_baudrate = true;
			ret = wrapper->SendCommandACKXF551();
		} else {
			ret = wrapper->SendCommandACK();
		}
		if (ret) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_CMD_ACK_FAILED();
			break;
		}

		size_t buflen = fImageConfig.GetSectorLength(sec);

		const char* description = 0;
		switch (frame.command) {
		case 0x50: description = "[ put sector ]"; break;
		case 0x57: description = "[ write sector ]"; break;
		case 0xd0: description = "[ put sector XF551 ]"; break;
		case 0xd7: description = "[ write sector XF551 ]"; break;
		}

		if ((ret=wrapper->ReceiveDataFrame(fBuffer, buflen))) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_RECEIVE_DATA_FAILED();
			break;
		}

		bool lastChanged = fImage->Changed();

		if (fImage->IsWriteProtected() ) {
			fLastFDCStatus = 0xbf; // write protected
			ret = AbstractSIOHandler::eWriteProtected;

			fTracer->TraceCommandError(ret);
			if ((frame.command & 0x7f) == 0x50) {
				fTracer->TraceWriteSector(myDriveNo, sec, hi_cmd);
			} else {
				fTracer->TraceWriteSectorVerify(myDriveNo, sec, hi_cmd);
			}
			fTracer->TraceDataBlock(fBuffer, buflen, description);

			if (wrapper->SendError()) {
				LOG_SIO_ERROR_FAILED();
				break;
			}
			break;
		} else {
			if (fVirtualImageObserver) {
				fVirtualImageObserver->IndicateBeforeSectorWrite(sec);
			}
			if (!fImage->WriteSector(sec, fBuffer, buflen)) {
				fLastFDCStatus = 0xb0; // write protected
				ret = AbstractSIOHandler::eImageError;

				fTracer->TraceCommandError(ret);
				if ((frame.command & 0x7f) == 0x50) {
					fTracer->TraceWriteSector(myDriveNo, sec, hi_cmd);
				} else {
					fTracer->TraceWriteSectorVerify(myDriveNo, sec, hi_cmd);
				}
				fTracer->TraceDataBlock(fBuffer, buflen, description);

				if (wrapper->SendError()) {
					LOG_SIO_ERROR_FAILED();
					break;
				}
				break;
			} else {
				fLastFDCStatus = 0xff;

				if ((lastChanged == false) && fImage->Changed()) {
					fTracer->IndicateDriveChanged(myDriveNo);
				}
				fTracer->TraceCommandOK();
				if ((frame.command & 0x7f) == 0x50) {
					fTracer->TraceWriteSector(myDriveNo, sec, hi_cmd);
				} else {
					fTracer->TraceWriteSectorVerify(myDriveNo, sec, hi_cmd);
				}
				fTracer->TraceDataBlock(fBuffer, buflen, description);

				if (fVirtualImageObserver) {
					fVirtualImageObserver->IndicateAfterSectorWrite(sec);
				}
				if (hi_cmd) {
					ret = wrapper->SendCompleteXF551();
					reset_baudrate = false;
				} else {
					ret = wrapper->SendComplete();
				}
				if (ret) {
					LOG_SIO_COMPLETE_FAILED();
					break;
				}
			}
		}
		break;
	}
	case 0xce:
	case 0x4e: {
		/* percom get */

		if (hi_cmd) {
			if (!fEnableXF551Mode) {
				if (wrapper->SendCommandNAK()) {
					LOG_SIO_CMD_NAK_FAILED();
				}

				ret = AbstractSIOHandler::eUnsupportedCommand;
				fTracer->TraceCommandError(ret);

				break;
			}
			reset_baudrate = true;
			ret = wrapper->SendCommandACKXF551();
		} else {
			ret = wrapper->SendCommandACK();
		}

		if (ret) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_CMD_ACK_FAILED();
			break;
		}

		size_t buflen = 12;
		unsigned int seclen;

		const char* description = 0;
		switch (frame.command) {
		case 0x4e: description = "[ percom get ]"; break;
		case 0xce: description = "[ percom get XF551 ]"; break;
		}

		fBuffer[0] = fFormatConfig.fTracksPerSide;
		fBuffer[1] = 0;
		fBuffer[2] = fFormatConfig.fSectorsPerTrack >> 8;
		fBuffer[3] = fFormatConfig.fSectorsPerTrack & 0xff;
		if (fFormatConfig.fSides) {
			fBuffer[4] = fFormatConfig.fSides - 1;
		} else {
			fBuffer[4] = 0;
		}
		if (fFormatConfig.fDiskFormat == e90kDisk) {
			fBuffer[5] = 0;
		} else {
			fBuffer[5] = 4;
		}
		seclen = fFormatConfig.GetSectorLength();
		fBuffer[6] = seclen >> 8;
		fBuffer[7] = seclen & 0xff;
		fBuffer[8] = 1;
		fBuffer[9] = 1;
		fBuffer[10] = 0;
		fBuffer[11] = 0;

		fTracer->TraceCommandOK();
		fTracer->TraceDecodedPercomBlock(myDriveNo, fBuffer, true, hi_cmd);
		fTracer->TraceDataBlock(fBuffer, buflen, description);

		if ((ret=wrapper->SendComplete())) {
			LOG_SIO_COMPLETE_FAILED();
			break;
		}

		if (hi_cmd) {
			ret = wrapper->SendDataFrameXF551(fBuffer, 12);
			reset_baudrate = false;
		} else {
			ret = wrapper->SendDataFrame(fBuffer, 12);
		}
		if (ret) {
			LOG_SIO_SEND_DATA_FAILED();
			break;
		}

		break;
	}
	case 0xcf:
	case 0x4f: {
		/* percom put */

		if (hi_cmd) {
			if (!fEnableXF551Mode) {
				if (wrapper->SendCommandNAK()) {
					LOG_SIO_CMD_NAK_FAILED();
				}

				ret = AbstractSIOHandler::eUnsupportedCommand;
				fTracer->TraceCommandError(ret);

				break;
			}
			reset_baudrate = true;
			ret = wrapper->SendCommandACKXF551();
		} else {
			ret = wrapper->SendCommandACK();
		}
		if (ret) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_CMD_ACK_FAILED();
			break;
		}

		size_t buflen = 12;

		const char* description = 0;
		switch (frame.command) {
		case 0x4f: description = "[ percom put ]"; break;
		case 0xcf: description = "[ percom put XF551 ]"; break;
		}

		if ((ret=wrapper->ReceiveDataFrame(fBuffer, 12)) ) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_RECEIVE_DATA_FAILED();
			break;
		}

		uint8_t tracks, sides;
		uint16_t sec, secLen;
		uint32_t total;

		tracks = fBuffer[0];
		sec = fBuffer[3] + (fBuffer[2] << 8);
		sides = fBuffer[4] + 1;
		secLen = fBuffer[7] + (fBuffer[6]<<8);

		total = tracks * sec * sides;
		if (VerifyPercomFormat(tracks, sides, sec, secLen, total)) {
			fFormatConfig.fTracksPerSide = tracks;
			fFormatConfig.fSectorsPerTrack = sec;
			fFormatConfig.fSides = sides;

			switch (secLen) {
			case 256: fFormatConfig.fSectorLength = e256BytesPerSector; break;
			case 512: fFormatConfig.fSectorLength = e512BytesPerSector; break;
			case 1024: fFormatConfig.fSectorLength = e1kPerSector; break;
			case 2048: fFormatConfig.fSectorLength = e2kPerSector; break;
			case 4096: fFormatConfig.fSectorLength = e4kPerSector; break;
			case 8192: fFormatConfig.fSectorLength = e8kPerSector; break;
			case 128:
			default:
				fFormatConfig.fSectorLength = e128BytesPerSector; break;
			}

			fFormatConfig.fNumberOfSectors = total;
			fFormatConfig.DetermineDiskFormatFromLayout();

			fTracer->TraceCommandOK();
			fTracer->TraceDecodedPercomBlock(myDriveNo, fBuffer, false, hi_cmd);
			fTracer->TraceDataBlock(fBuffer, buflen, description);

			if (hi_cmd) {
				ret = wrapper->SendCompleteXF551();
				reset_baudrate = false;
			} else {
				ret = wrapper->SendComplete();
			}
			if (ret) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}
		} else {
			ret = AbstractSIOHandler::eInvalidPercomConfig;

			fTracer->TraceCommandError(ret);
			LOG_SIO_MISC("invalid config: %d sectors, %d bytes per sector",total,secLen);
			fTracer->TraceDecodedPercomBlock(myDriveNo, fBuffer, false, hi_cmd);
			fTracer->TraceDataBlock(fBuffer, buflen, description);

			if (wrapper->SendError()) {
				LOG_SIO_ERROR_FAILED();
				break;
			}
		}

		break;

	}

	case 0x20: {
		// format disk automatically (speedy 1050)

		if (!fEnableHighSpeed || IsVirtualImage()) {
			if (wrapper->SendCommandNAK()) {
				LOG_SIO_CMD_NAK_FAILED();
			}

			ret = AbstractSIOHandler::eUnsupportedCommand;
			fTracer->TraceCommandError(ret);

			break;
		}
		if ((ret=wrapper->SendCommandACK()) ) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_CMD_ACK_FAILED();
			break;
		}

		if ( fImage->IsWriteProtected()) {
			fLastFDCStatus = 0xbf; // write protected
			ret = AbstractSIOHandler::eWriteProtected;
			fTracer->TraceCommandError(ret);
			if (wrapper->SendError()) {
				LOG_SIO_ERROR_FAILED();
				break;
			}
			break;
		}

		bool lastChanged = fImage->Changed();

		if (!fImage->CreateImage(fFormatConfig.fSectorLength,
					fFormatConfig.fSectorsPerTrack,
					fFormatConfig.fTracksPerSide,
					fFormatConfig.fSides)) {
			fLastFDCStatus = 0xbf; // write protected
			ret = AbstractSIOHandler::eImageError;
			fTracer->TraceCommandError(ret);
			DPRINTF("create image failed [%d %d]",
				fFormatConfig.fSectorLength, fFormatConfig.fNumberOfSectors);
			if (wrapper->SendError()) {
				LOG_SIO_ERROR_FAILED();
				break;
			}
			break;
		} else {
			fLastFDCStatus = 0xff;
			fImageConfig = fImage->GetImageConfig();
			fFormatConfig = fImageConfig;

			if ((lastChanged == false) && fImage->Changed()) {
				fTracer->IndicateDriveChanged(myDriveNo);
			}
			fTracer->IndicateDriveFormatted(myDriveNo);
			fTracer->TraceCommandOK();

			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}
		}

		break;
	}
	case 0xa1:
	case 0x21: {
		// format disk

		if (hi_cmd && !fEnableXF551Mode) {
			if (wrapper->SendCommandNAK()) {
				LOG_SIO_CMD_NAK_FAILED();
			}

			ret = AbstractSIOHandler::eUnsupportedCommand;
			fTracer->TraceCommandError(ret);

			break;
		}
		if ((ret=wrapper->SendCommandACK())) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_CMD_ACK_FAILED();
			break;
		}

		size_t buflen = fFormatConfig.fSectorLength;

		const char* description = 0;
		switch (frame.command) {
		case 0x21: description = "[ format disk ]"; break;
		case 0xa1: description = "[ format disk XF551 ]"; break;
		}

		if (fImage->IsWriteProtected() || IsVirtualImage() ) {
			fLastFDCStatus = 0xbf; // write protected
			ret = AbstractSIOHandler::eWriteProtected;
			memset(fBuffer,0,buflen);

			fTracer->TraceCommandError(ret);
			fTracer->TraceFormatDisk(myDriveNo, hi_cmd);
			fTracer->TraceDataBlock(fBuffer, buflen, description);

			if (wrapper->SendError()) {
				LOG_SIO_ERROR_FAILED();
				break;
			}
		} else {
			bool lastChanged = fImage->Changed();

			if (!fImage->CreateImage(fFormatConfig.fSectorLength,
						fFormatConfig.fSectorsPerTrack,
						fFormatConfig.fTracksPerSide,
						fFormatConfig.fSides)) {
				fLastFDCStatus = 0xbf; // write protected
				ret = AbstractSIOHandler::eImageError;
				memset(fBuffer,0,buflen);

				fTracer->TraceCommandError(ret);
				fTracer->TraceFormatDisk(myDriveNo, hi_cmd);
				fTracer->TraceDataBlock(fBuffer, buflen, description);

				DPRINTF("create image failed [%d %d]",
					fFormatConfig.fSectorLength, fFormatConfig.fNumberOfSectors);
				if (wrapper->SendError()) {
					LOG_SIO_ERROR_FAILED();
					break;
				}
			} else {
				fLastFDCStatus = 0xff;
				fImageConfig = fImage->GetImageConfig();
				fFormatConfig = fImageConfig;
				memset(fBuffer,255,buflen);

				if ((lastChanged == false) && fImage->Changed()) {
					fTracer->IndicateDriveChanged(myDriveNo);
				}
				fTracer->IndicateDriveFormatted(myDriveNo);

				fTracer->TraceCommandOK();
				fTracer->TraceFormatDisk(myDriveNo, hi_cmd);
				fTracer->TraceDataBlock(fBuffer, buflen, description);

				if ((ret=wrapper->SendComplete())) {
					LOG_SIO_COMPLETE_FAILED();
					break;
				}
			}
		}

		if ((ret2=wrapper->SendDataFrame(fBuffer, buflen))) {
			LOG_SIO_SEND_DATA_FAILED();
			if (ret==0) ret=ret2;
			break;
		}

		break;
	}

	case 0xa2:
	case 0x22: {
		// format disk in enhanced density

		if (hi_cmd && !fEnableXF551Mode) {
			if (wrapper->SendCommandNAK()) {
				LOG_SIO_CMD_NAK_FAILED();
			}

			ret = AbstractSIOHandler::eUnsupportedCommand;
			fTracer->TraceCommandError(ret);

			break;
		}

		if ((ret=wrapper->SendCommandACK())) {
			LOG_SIO_CMD_ACK_FAILED();
			break;
		}

		size_t buflen = 128;

		const char* description = 0;
		switch (frame.command) {
		case 0x22: description = "[ format enhanced density ]"; break;
		case 0xa2: description = "[ format enhanced density XF551 ]"; break;
		}

		if ( fImage->IsWriteProtected() || IsVirtualImage() ) {
			fLastFDCStatus = 0xbf; // write protected
			ret = AbstractSIOHandler::eWriteProtected;
			memset(fBuffer,0,128);

			fTracer->TraceCommandError(ret);
			fTracer->TraceFormatEnhanced(myDriveNo, hi_cmd);
			fTracer->TraceDataBlock(fBuffer, buflen, description);

			if (wrapper->SendError()) {
				LOG_SIO_ERROR_FAILED();
				break;
			}
		} else {
			bool lastChanged = fImage->Changed();

			if (!fImage->CreateImage(e130kDisk)) {
				fLastFDCStatus = 0xbf; // write protected
				ret = AbstractSIOHandler::eImageError;
				memset(fBuffer,0,128);

				fTracer->TraceCommandError(ret);
				fTracer->TraceFormatEnhanced(myDriveNo, hi_cmd);
				fTracer->TraceDataBlock(fBuffer, buflen, description);

				if (wrapper->SendError()) {
					LOG_SIO_ERROR_FAILED();
					break;
				}
			} else {
				fLastFDCStatus = 0xff;
				fImageConfig = fImage->GetImageConfig();
				fFormatConfig = fImageConfig;
				memset(fBuffer,255,128);

				if ((lastChanged == false) && fImage->Changed()) {
					fTracer->IndicateDriveChanged(myDriveNo);
				}
				fTracer->IndicateDriveFormatted(myDriveNo);

				fTracer->TraceCommandOK();
				fTracer->TraceFormatEnhanced(myDriveNo, hi_cmd);
				fTracer->TraceDataBlock(fBuffer, buflen, description);

				if ((ret=wrapper->SendComplete())) {
					LOG_SIO_COMPLETE_FAILED();
					break;
				}
			}
		}

		if ((ret2=wrapper->SendDataFrame(fBuffer, 128))) {
			LOG_SIO_SEND_DATA_FAILED();
			if (ret==0) ret=ret2;
			break;
		}

		break;
	}

	case 0x3f: {
		// get ultra speed byte

		if (fEnableHighSpeed) {

			size_t buflen = 1;
			const char* description = "[ get speed byte ]";

			fBuffer[0] = fSpeedByte;

			if ((ret=wrapper->SendCommandACK())) {
				fTracer->TraceCommandError(ret);
				LOG_SIO_CMD_ACK_FAILED();
				break;
			}

			fTracer->TraceCommandOK();
			fTracer->TraceGetSpeedByte(myDriveNo);
			fTracer->TraceDataBlock(fBuffer, buflen, description);
	
			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}

			if ((ret=wrapper->SendDataFrame(fBuffer, 1))) {
				LOG_SIO_SEND_DATA_FAILED();
				break;
			}
		} else {
			if (wrapper->SendCommandNAK()) {
				LOG_SIO_CMD_NAK_FAILED();
			}

			ret = AbstractSIOHandler::eUnsupportedCommand;
			fTracer->TraceCommandError(ret);
		}

		break;
	}
	case 0x68: {
		// get SIO length

		if (fEnableHighSpeed) {

			size_t buflen = 2;
			const char * description = "[ get SIO length ]";

			if ((ret=wrapper->SendCommandACK())) {
				fTracer->TraceCommandError(ret);
				LOG_SIO_CMD_ACK_FAILED();
				break;
			}
	
			size_t codelen = HighSpeedSIOCode::GetInstance()->GetCodeSize();

			fBuffer[0] = codelen & 0xff;
			fBuffer[1] = codelen >> 8;

			fTracer->TraceCommandOK(),
			fTracer->TraceGetSioCodeLength(myDriveNo);
			fTracer->TraceDataBlock(fBuffer, buflen, description);

			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}

			if ((ret=wrapper->SendDataFrame(fBuffer, 2))) {
				LOG_SIO_SEND_DATA_FAILED();
				break;
			}

		} else {
			if (wrapper->SendCommandNAK()) {
				LOG_SIO_CMD_NAK_FAILED();
			}
			ret = AbstractSIOHandler::eUnsupportedCommand;
			fTracer->TraceCommandError(ret);
		}

		break;
	}
	case 0x69: {
		// get SIO code

		if (fEnableHighSpeed) {

			size_t buflen = HighSpeedSIOCode::GetInstance()->GetCodeSize();
			if (buflen > eBufferSize) {
				DPRINTF("internal error, buffer for highspeed code too small!");
				ret = wrapper->SendCommandNAK();
				break;
			}

			const char* description = "[ get SIO code ]";

			if ((ret=wrapper->SendCommandACK())) {
				fTracer->TraceCommandError(ret);
				LOG_SIO_CMD_ACK_FAILED();
				break;
			}
	
			uint16_t relocadr = frame.aux1 + (frame.aux2<<8);

			HighSpeedSIOCode::GetInstance()->RelocateCode(fBuffer, relocadr);

			fTracer->TraceCommandOK();
			fTracer->TraceGetSioCode(myDriveNo);
			fTracer->TraceDataBlock(fBuffer, buflen, description);

			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}

			if ((ret=wrapper->SendDataFrame(fBuffer, buflen))) {
				LOG_SIO_SEND_DATA_FAILED();
				break;
			}

		} else {
			if (wrapper->SendCommandNAK()) {
				LOG_SIO_CMD_NAK_FAILED();
			}
			ret = AbstractSIOHandler::eUnsupportedCommand;
			fTracer->TraceCommandError(ret);
		}

		break;
	}
	case 0x44: // configure drive
	case 0x4b: // slow/fast config
	case 0x51: // flush disks
	{
		// just ack these commands, but ignore them.

		if (fEnableHighSpeed) {

			if ((ret=wrapper->SendCommandACK())) {
				fTracer->TraceCommandError(ret);
				LOG_SIO_CMD_ACK_FAILED();
				break;
			}
	
			fTracer->TraceCommandOK();

			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}

		} else {
			if (wrapper->SendCommandNAK()) {
				LOG_SIO_CMD_NAK_FAILED();
			}
			ret = AbstractSIOHandler::eUnsupportedCommand;
			fTracer->TraceCommandError(ret);
		}

		break;
	}
	case 0x6d: {
		/* read MyPicoDos code */
		uint16_t sec = frame.aux1 + (frame.aux2<<8);
		if ( ! MyPicoDosCode::SectorNumberOK(sec)) {
			if (wrapper->SendCommandNAK()) {
				LOG_SIO_CMD_NAK_FAILED();
			}

			ret = AbstractSIOHandler::eIllegalSectorNumber;

			fTracer->TraceCommandError(ret);
			LOG_SIO_MISC("illegal MyPicoDos sector number %d",sec);

			break;
		}

		if ((ret=wrapper->SendCommandACK())) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_CMD_ACK_FAILED();
			break;
		}

		size_t buflen = 128;
		const char* description = "[ read MyPicoDos ]";
		MyPicoDosCode* mypdos = MyPicoDosCode::GetInstance();

		if (!mypdos->GetMyPicoDosSector(sec, fBuffer, buflen)) {
			ret = AbstractSIOHandler::eImageError;

			fTracer->TraceCommandError(ret);
			fTracer->TraceReadMyPicoDos(myDriveNo, sec);
			fTracer->TraceDataBlock(fBuffer, buflen, description);

			if (wrapper->SendError()) {
				LOG_SIO_ERROR_FAILED();
				break;
			}
		} else {
			fTracer->TraceCommandOK();
			fTracer->TraceReadMyPicoDos(myDriveNo, sec);
			fTracer->TraceDataBlock(fBuffer, buflen, description);

			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}
		}

		if ((ret2=wrapper->SendDataFrame(fBuffer, buflen))) {
			LOG_SIO_SEND_DATA_FAILED();
			if (ret==0) ret=ret2;
			break;
		}
		break;
	}
	case 0x93: { // ape special commands
		/*
		if (!fEnableHighSpeed) {
			goto unsupported_command;
		}
		*/
		switch (frame.aux1) {
		case 0xee: {
			if (frame.aux2 != 0xa0) {
				goto unsupported_command;
			}
			// ape time

			size_t buflen = 6;
			const char* shortdesc = "get time";
			const char* description = "[ get APE time ]";

			if ((ret=wrapper->SendCommandACK())) {
				fTracer->TraceCommandError(ret);
				LOG_SIO_CMD_ACK_FAILED();
				break;
			}

			time_t current_time;
			struct tm* ltime;
			time(&current_time);
			ltime = localtime(&current_time);

			fBuffer[0] = ltime->tm_mday;
			fBuffer[1] = ltime->tm_mon+1;
			fBuffer[2] = ltime->tm_year % 100;
			fBuffer[3] = ltime->tm_hour;
			fBuffer[4] = ltime->tm_min;
			fBuffer[5] = ltime->tm_sec;

			fTracer->TraceCommandOK();
			fTracer->TraceApeSpecial(myDriveNo, shortdesc);
			fTracer->TraceDataBlock(fBuffer, buflen, description);
	
			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}

			if ((ret=wrapper->SendDataFrame(fBuffer, buflen))) {
				LOG_SIO_SEND_DATA_FAILED();
				break;
			}
			break;
		}

		case 0xf0: { // get image filename
			size_t buflen = 256;
			const char* shortdesc = "get image name";
			const char* description = "[ get APE image name ]";

			if ((ret=wrapper->SendCommandACK())) {
				fTracer->TraceCommandError(ret);
				LOG_SIO_CMD_ACK_FAILED();
				break;
			}

			memset(fBuffer, 0, buflen);
			char* fn = MiscUtils::ShortenFilename(fImage->GetFilename(), buflen - 2);
			if (fn) {
				snprintf((char*)fBuffer, buflen-1, "%s", fn);
				delete[] fn;
			}
			if (fImage->IsVirtualImage()) {
				fBuffer[255] = 'M';
			} else {
				fBuffer[255] = 'A';
			}

			fTracer->TraceCommandOK();
			fTracer->TraceApeSpecial(myDriveNo, shortdesc);
			fTracer->TraceDataBlock(fBuffer, buflen, description);
	
			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}

			if ((ret=wrapper->SendDataFrame(fBuffer, buflen))) {
				LOG_SIO_SEND_DATA_FAILED();
				break;
			}
			break;
		}
		case 0xf1: { // get version
			size_t buflen = 256;
			const char* shortdesc = "get version";
			const char* description = "[ get APE version ]";

			if ((ret=wrapper->SendCommandACK())) {
				fTracer->TraceCommandError(ret);
				LOG_SIO_CMD_ACK_FAILED();
				break;
			}

			memset(fBuffer, 0, buflen);
			snprintf((char*)fBuffer, buflen - 1, "AtariSIO for Linux" VERSION_STRING "\233"
					"(c) 2003-2008 Matthias Reichl\233");

			fTracer->TraceCommandOK();
			fTracer->TraceApeSpecial(myDriveNo, shortdesc);
			fTracer->TraceDataBlock(fBuffer, buflen, description);
	
			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}

			if ((ret=wrapper->SendDataFrame(fBuffer, buflen))) {
				LOG_SIO_SEND_DATA_FAILED();
				break;
			}
			break;
		}
		default:
			   goto unsupported_command;
		}
		break;
	}
/*
	case 0x4c: // speedy: jump to address
	{
		// routine at $ff5a sends ACK

		if (fEnableHighSpeed && 
			(frame.aux1 == 0x5a) && (frame.aux2 == 0xff) ) {

			if ((ret=wrapper->SendCommandACK())) {
				fTracer->TraceCommandError(ret);
				LOG_SIO_CMD_ACK_FAILED();
				break;
			}
	
			fTracer->TraceCommandOK();

			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}

		} else {
			if (wrapper->SendCommandNAK()) {
				LOG_SIO_CMD_NAK_FAILED();
			}
			ret = AbstractSIOHandler::eUnsupportedCommand;
			fTracer->TraceCommandError(ret);
		}

		break;
	}
	case 0x41: {
		// Speedy: add new command
		// do nothing, just ACK it

		if (fEnableHighSpeed) {
			if ((ret=wrapper->SendCommandACK()) ) {
				LOG_SIO_CMD_ACK_FAILED();
				break;
			}

			size_t buflen = 3;
			const char* description = "[ add command ]";

			if ((ret=wrapper->ReceiveDataFrame(fBuffer, buflen)) ) {
				fTracer->TraceCommandError(ret);
				LOG_SIO_RECEIVE_DATA_FAILED();
				break;
			}

			fTracer->TraceCommandOK();
			fTracer->TraceDataBlock(fBuffer, buflen, description);

			if ((ret=wrapper->SendComplete()) ) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}
		} else {
			if (wrapper->SendCommandNAK()) {
				LOG_SIO_CMD_NAK_FAILED();
			}
			ret = AbstractSIOHandler::eUnsupportedCommand;
			fTracer->TraceCommandError(ret);
		}

		break;
	}
*/
	default:
unsupported_command:
		if (wrapper->SendCommandNAK()) {
			LOG_SIO_CMD_NAK_FAILED();
		}
		ret = AbstractSIOHandler::eUnsupportedCommand;
		fTracer->TraceCommandError(ret);
		break;
	}

	if (reset_baudrate) {
		wrapper->SetBaudrate(wrapper->GetStandardBaudrate(), false);
		LOG_SIO_MISC("resetting baudrate");
	}

	return ret;
}

bool AtrSIOHandler::IsAtrSIOHandler() const
{
	return true;
}

bool AtrSIOHandler::EnableHighSpeed(bool on)
{
	fEnableHighSpeed = on;
/*
	if (on) {
		fSpeedByte = SPEED_BYTE_57600;
	} else {
		fSpeedByte = SPEED_BYTE_19200;
	}
*/
	return true;
}

bool AtrSIOHandler::EnableXF551Mode(bool on)
{
	fEnableXF551Mode = on;
	return true;
}

bool AtrSIOHandler::SetHighSpeedParameters(unsigned int pokeyDivisor, unsigned int baudrate)
{
	if (pokeyDivisor > 255) {
		return false;
	}
	fSpeedByte = pokeyDivisor;
	fHighSpeedBaudrate = baudrate;
	return true;
}

bool AtrSIOHandler::EnableStrictFormatChecking(bool on)
{
	fStrictFormatChecking = on;
	return true;
}

bool AtrSIOHandler::VerifyPercomFormat(uint8_t tracks, uint8_t sides, uint16_t sectors, uint16_t seclen, uint32_t total_sectors) const
{
	if (total_sectors == 0 || total_sectors >= 65536) {
		return false;
	}
	if (seclen != 128 && seclen != 256 && seclen != 512 &&
	    seclen != 1024 && seclen != 2048 && seclen != 4096 && seclen != 8192) {
		return false;
	}
	if (fStrictFormatChecking) {
		if (sides != 1 || tracks != 40) {
			return false;
		}
		switch (seclen) {
		case 128:
			return (sectors == 18) || (sectors == 26);
			break;
		case 256:
			return (sectors == 18);
		default:
			return false;
		}
	}
	return true;
}
