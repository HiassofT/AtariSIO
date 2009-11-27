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

AtrSIOHandler::AtrSIOHandler(const RCPtr<AtrImage>& image)
	: fImage(image),
	  fEnableHighSpeed(false),
	  fEnableXF551Mode(false),

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

	unsigned int myDriveNo = frame.device_id - 0x30;
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

		unsigned int buflen = 4;
		uint8_t buf[buflen];

		const char* description = 0;
		switch (frame.command) {
		case 0x53: description = "[ get status ]"; break;
		case 0xd3: description = "[ get status XF551 ]"; break;
		}

		buf[0] = 0x10; // motor on
		if (fFormatConfig.fSectorLength == e128BytesPerSector) {
			if (fFormatConfig.fNumberOfSectors==1040) {
				buf[0] |= 0x80; /* enhanced density */
			}
		} else {
			buf[0] |= 0x20; /* double density */
			if (fEnableXF551Mode && (fFormatConfig.fNumberOfSectors == 1440) ) {
				buf[0] |= 0x80; /* XF551 QD sets both DD and ED flags */
			}
		}
		buf[1] = fLastFDCStatus;
		if (fEnableXF551Mode) {
			buf[2] = 0xfe;
		} else {
			buf[2] = 0xe0;
		}
		buf[3] = 0;

		if (fImage->IsWriteProtected()) {
			buf[0] |= 0x08;
		}

		fTracer->TraceCommandOK();
		fTracer->TraceGetStatus(myDriveNo, hi_cmd);
		fTracer->TraceDataBlock(buf, 4, description);

		if ((ret=wrapper->SendComplete())) {
			LOG_SIO_COMPLETE_FAILED();
			break;
		}

		if (hi_cmd) {
			ret = wrapper->SendDataFrameXF551(buf, 4);
			reset_baudrate = false;
		} else {
			ret = wrapper->SendDataFrame(buf, 4);
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

		unsigned int sec = frame.aux1 + (frame.aux2<<8);
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

		unsigned int buflen = fImageConfig.GetSectorLength(sec);
		uint8_t buf[buflen];

		const char* description = 0;
		switch (frame.command) {
		case 0x52: description = "[ read sector ]"; break;
		case 0xd2: description = "[ read sector XF551 ]"; break;
		}

		if (!fImage->ReadSector(sec, buf, buflen)) {
			fLastFDCStatus = 0xef; // record not found;
			ret = AbstractSIOHandler::eImageError;

			fTracer->TraceCommandError(ret);
			fTracer->TraceReadSector(myDriveNo, sec, hi_cmd);
			fTracer->TraceDataBlock(buf, buflen, description);

			if (wrapper->SendError()) {
				LOG_SIO_ERROR_FAILED();
				break;
			}
		} else {
			fLastFDCStatus = 0xff;

			fTracer->TraceCommandOK();
			fTracer->TraceReadSector(myDriveNo, sec, hi_cmd);
			fTracer->TraceDataBlock(buf, buflen, description);

			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}
		}

		if (hi_cmd) {
			ret2 = wrapper->SendDataFrameXF551(buf, buflen);
			reset_baudrate = false;
		} else {
			ret2 = wrapper->SendDataFrame(buf, buflen);
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
		unsigned int sec = frame.aux1 + (frame.aux2<<8);
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

		unsigned int buflen = fImageConfig.GetSectorLength(sec);
		uint8_t buf[buflen];

		const char* description = 0;
		switch (frame.command) {
		case 0x50: description = "[ put sector ]"; break;
		case 0x57: description = "[ write sector ]"; break;
		case 0xd0: description = "[ put sector XF551 ]"; break;
		case 0xd7: description = "[ write sector XF551 ]"; break;
		}

		if ((ret=wrapper->ReceiveDataFrame(buf, buflen))) {
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
			fTracer->TraceDataBlock(buf, buflen, description);

			if (wrapper->SendError()) {
				LOG_SIO_ERROR_FAILED();
				break;
			}
			break;
		} else {
			if (fVirtualImageObserver) {
				fVirtualImageObserver->IndicateBeforeSectorWrite(sec);
			}
			if (!fImage->WriteSector(sec, buf, buflen)) {
				fLastFDCStatus = 0xb0; // write protected
				ret = AbstractSIOHandler::eImageError;

				fTracer->TraceCommandError(ret);
				if ((frame.command & 0x7f) == 0x50) {
					fTracer->TraceWriteSector(myDriveNo, sec, hi_cmd);
				} else {
					fTracer->TraceWriteSectorVerify(myDriveNo, sec, hi_cmd);
				}
				fTracer->TraceDataBlock(buf, buflen, description);

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
				fTracer->TraceDataBlock(buf, buflen, description);

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

		unsigned int buflen = 12;
		uint8_t buf[buflen];

		const char* description = 0;
		switch (frame.command) {
		case 0x4e: description = "[ percom get ]"; break;
		case 0xce: description = "[ percom get XF551 ]"; break;
		}

		buf[0] = fFormatConfig.fTracksPerSide;
		buf[1] = 0;
		buf[2] = fFormatConfig.fSectorsPerTrack >> 8;
		buf[3] = fFormatConfig.fSectorsPerTrack & 0xff;
		if (fFormatConfig.fSides) {
			buf[4] = fFormatConfig.fSides - 1;
		} else {
			buf[4] = 0;
		}
		if (fFormatConfig.fDiskFormat == e90kDisk) {
			buf[5] = 0;
		} else {
			buf[5] = 4;
		}
		if (fFormatConfig.fSectorLength == e256BytesPerSector) {
			buf[6] = 1;
			buf[7] = 0;
		} else {
			buf[6] = 0;
			buf[7] = 128;
		}
		buf[8] = 1;
		buf[9] = 1;
		buf[10] = 0;
		buf[11] = 0;

		fTracer->TraceCommandOK();
		fTracer->TraceDecodedPercomBlock(myDriveNo, buf, true, hi_cmd);
		fTracer->TraceDataBlock(buf, buflen, description);

		if ((ret=wrapper->SendComplete())) {
			LOG_SIO_COMPLETE_FAILED();
			break;
		}

		if (hi_cmd) {
			ret = wrapper->SendDataFrameXF551(buf, 12);
			reset_baudrate = false;
		} else {
			ret = wrapper->SendDataFrame(buf, 12);
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

		unsigned int buflen = 12;
		uint8_t buf[buflen];

		const char* description = 0;
		switch (frame.command) {
		case 0x4f: description = "[ percom put ]"; break;
		case 0xcf: description = "[ percom put XF551 ]"; break;
		}

		if ((ret=wrapper->ReceiveDataFrame(buf, 12)) ) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_RECEIVE_DATA_FAILED();
			break;
		}

		unsigned int tracks, sec, sides, total, secLen;

		tracks = buf[0];
		sec = buf[3] + (buf[2] << 8);
		sides = buf[4] + 1;
		secLen = buf[7] + (buf[6]<<8);

		total = tracks * sec * sides;
		if (total > 0 && total < 65536 &&
		    (secLen == 128 || secLen == 256) ) {
			fFormatConfig.fTracksPerSide = tracks;
			fFormatConfig.fSectorsPerTrack = sec;
			fFormatConfig.fSides = sides;
			if (secLen == 256) {
				fFormatConfig.fSectorLength = e256BytesPerSector;
			} else {
				fFormatConfig.fSectorLength = e128BytesPerSector;
			}

			fFormatConfig.fNumberOfSectors = total;
			fFormatConfig.DetermineDiskFormatFromLayout();

			fTracer->TraceCommandOK();
			fTracer->TraceDecodedPercomBlock(myDriveNo, buf, false, hi_cmd);
			fTracer->TraceDataBlock(buf, buflen, description);

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
			fTracer->TraceDecodedPercomBlock(myDriveNo, buf, false, hi_cmd);
			fTracer->TraceDataBlock(buf, buflen, description);

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

		unsigned int buflen = fFormatConfig.fSectorLength;
		uint8_t buf[buflen];

		const char* description = 0;
		switch (frame.command) {
		case 0x21: description = "[ format disk ]"; break;
		case 0xa1: description = "[ format disk XF551 ]"; break;
		}

		if (fImage->IsWriteProtected() || IsVirtualImage() ) {
			fLastFDCStatus = 0xbf; // write protected
			ret = AbstractSIOHandler::eWriteProtected;
			memset(buf,0,buflen);

			fTracer->TraceCommandError(ret);
			fTracer->TraceFormatDisk(myDriveNo, hi_cmd);
			fTracer->TraceDataBlock(buf, buflen, description);

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
				memset(buf,0,buflen);

				fTracer->TraceCommandError(ret);
				fTracer->TraceFormatDisk(myDriveNo, hi_cmd);
				fTracer->TraceDataBlock(buf, buflen, description);

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
				memset(buf,255,buflen);

				if ((lastChanged == false) && fImage->Changed()) {
					fTracer->IndicateDriveChanged(myDriveNo);
				}
				fTracer->IndicateDriveFormatted(myDriveNo);

				fTracer->TraceCommandOK();
				fTracer->TraceFormatDisk(myDriveNo, hi_cmd);
				fTracer->TraceDataBlock(buf, buflen, description);

				if ((ret=wrapper->SendComplete())) {
					LOG_SIO_COMPLETE_FAILED();
					break;
				}
			}
		}

		if ((ret2=wrapper->SendDataFrame(buf, buflen))) {
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

		unsigned int buflen = 128;
		uint8_t buf[buflen];

		const char* description = 0;
		switch (frame.command) {
		case 0x22: description = "[ format enhanced density ]"; break;
		case 0xa2: description = "[ format enhanced density XF551 ]"; break;
		}

		if ( fImage->IsWriteProtected() || IsVirtualImage() ) {
			fLastFDCStatus = 0xbf; // write protected
			ret = AbstractSIOHandler::eWriteProtected;
			memset(buf,0,128);

			fTracer->TraceCommandError(ret);
			fTracer->TraceFormatEnhanced(myDriveNo, hi_cmd);
			fTracer->TraceDataBlock(buf, buflen, description);

			if (wrapper->SendError()) {
				LOG_SIO_ERROR_FAILED();
				break;
			}
		} else {
			bool lastChanged = fImage->Changed();

			if (!fImage->CreateImage(e130kDisk)) {
				fLastFDCStatus = 0xbf; // write protected
				ret = AbstractSIOHandler::eImageError;
				memset(buf,0,128);

				fTracer->TraceCommandError(ret);
				fTracer->TraceFormatEnhanced(myDriveNo, hi_cmd);
				fTracer->TraceDataBlock(buf, buflen, description);

				if (wrapper->SendError()) {
					LOG_SIO_ERROR_FAILED();
					break;
				}
			} else {
				fLastFDCStatus = 0xff;
				fImageConfig = fImage->GetImageConfig();
				fFormatConfig = fImageConfig;
				memset(buf,255,128);

				if ((lastChanged == false) && fImage->Changed()) {
					fTracer->IndicateDriveChanged(myDriveNo);
				}
				fTracer->IndicateDriveFormatted(myDriveNo);

				fTracer->TraceCommandOK();
				fTracer->TraceFormatEnhanced(myDriveNo, hi_cmd);
				fTracer->TraceDataBlock(buf, buflen, description);

				if ((ret=wrapper->SendComplete())) {
					LOG_SIO_COMPLETE_FAILED();
					break;
				}
			}
		}

		if ((ret2=wrapper->SendDataFrame(buf, 128))) {
			LOG_SIO_SEND_DATA_FAILED();
			if (ret==0) ret=ret2;
			break;
		}

		break;
	}

	case 0x3f: {
		// get ultra speed byte

		if (fEnableHighSpeed) {

			unsigned int buflen = 1;
			uint8_t buf[buflen];
			const char* description = "[ get speed byte ]";

			buf[0] = fSpeedByte;

			if ((ret=wrapper->SendCommandACK())) {
				fTracer->TraceCommandError(ret);
				LOG_SIO_CMD_ACK_FAILED();
				break;
			}

			fTracer->TraceCommandOK();
			fTracer->TraceGetSpeedByte(myDriveNo);
			fTracer->TraceDataBlock(buf, buflen, description);
	
			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}

			if ((ret=wrapper->SendDataFrame(buf, 1))) {
				LOG_SIO_SEND_DATA_FAILED();
				break;
			}

			// set highspeed baudrate now, so the kernel driver doesn't
			// have to detect the baudrate switch
			wrapper->SetBaudrate(fHighSpeedBaudrate);

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

			unsigned int buflen = 2;
			uint8_t buf[buflen];
			const char * description = "[ get SIO length ]";

			if ((ret=wrapper->SendCommandACK())) {
				fTracer->TraceCommandError(ret);
				LOG_SIO_CMD_ACK_FAILED();
				break;
			}
	
			unsigned int codelen = HighSpeedSIOCode::GetInstance()->GetCodeSize();

			buf[0] = codelen & 0xff;
			buf[1] = codelen >> 8;

			fTracer->TraceCommandOK(),
			fTracer->TraceGetSioCodeLength(myDriveNo);
			fTracer->TraceDataBlock(buf, buflen, description);

			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}

			if ((ret=wrapper->SendDataFrame(buf, 2))) {
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

			unsigned int buflen = HighSpeedSIOCode::GetInstance()->GetCodeSize();
			uint8_t buf[buflen];
			const char* description = "[ get SIO code ]";

			if ((ret=wrapper->SendCommandACK())) {
				fTracer->TraceCommandError(ret);
				LOG_SIO_CMD_ACK_FAILED();
				break;
			}
	
			unsigned int relocadr = frame.aux1 + (frame.aux2<<8);

			HighSpeedSIOCode::GetInstance()->RelocateCode(buf, relocadr);

			fTracer->TraceCommandOK();
			fTracer->TraceGetSioCode(myDriveNo);
			fTracer->TraceDataBlock(buf, buflen, description);

			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}

			if ((ret=wrapper->SendDataFrame(buf, buflen))) {
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
		unsigned int sec = frame.aux1 + (frame.aux2<<8);
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

		unsigned int buflen = 128;
		uint8_t buf[buflen];
		const char* description = "[ read MyPicoDos ]";
		MyPicoDosCode* mypdos = MyPicoDosCode::GetInstance();

		if (!mypdos->GetMyPicoDosSector(sec, buf, buflen)) {
			ret = AbstractSIOHandler::eImageError;

			fTracer->TraceCommandError(ret);
			fTracer->TraceReadMyPicoDos(myDriveNo, sec);
			fTracer->TraceDataBlock(buf, buflen, description);

			if (wrapper->SendError()) {
				LOG_SIO_ERROR_FAILED();
				break;
			}
		} else {
			fTracer->TraceCommandOK();
			fTracer->TraceReadMyPicoDos(myDriveNo, sec);
			fTracer->TraceDataBlock(buf, buflen, description);

			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}
		}

		if ((ret2=wrapper->SendDataFrame(buf, buflen))) {
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

			unsigned int buflen = 6;
			uint8_t buf[buflen];
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

			buf[0] = ltime->tm_mday;
			buf[1] = ltime->tm_mon+1;
			buf[2] = ltime->tm_year % 100;
			buf[3] = ltime->tm_hour;
			buf[4] = ltime->tm_min;
			buf[5] = ltime->tm_sec;

			fTracer->TraceCommandOK();
			fTracer->TraceApeSpecial(myDriveNo, shortdesc);
			fTracer->TraceDataBlock(buf, buflen, description);
	
			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}

			if ((ret=wrapper->SendDataFrame(buf, buflen))) {
				LOG_SIO_SEND_DATA_FAILED();
				break;
			}
			break;
		}

		case 0xf0: { // get image filename
			unsigned int buflen = 256;
			uint8_t buf[buflen];
			const char* shortdesc = "get image name";
			const char* description = "[ get APE image name ]";

			if ((ret=wrapper->SendCommandACK())) {
				fTracer->TraceCommandError(ret);
				LOG_SIO_CMD_ACK_FAILED();
				break;
			}

			memset(buf, 0, buflen);
			char* fn = MiscUtils::ShortenFilename(fImage->GetFilename(), buflen - 2);
			if (fn) {
				snprintf((char*)buf, buflen-1, "%s", fn);
				delete[] fn;
			}
			if (fImage->IsVirtualImage()) {
				buf[255] = 'M';
			} else {
				buf[255] = 'A';
			}

			fTracer->TraceCommandOK();
			fTracer->TraceApeSpecial(myDriveNo, shortdesc);
			fTracer->TraceDataBlock(buf, buflen, description);
	
			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}

			if ((ret=wrapper->SendDataFrame(buf, buflen))) {
				LOG_SIO_SEND_DATA_FAILED();
				break;
			}
			break;
		}
		case 0xf1: { // get version
			unsigned int buflen = 256;
			uint8_t buf[buflen];
			const char* shortdesc = "get version";
			const char* description = "[ get APE version ]";

			if ((ret=wrapper->SendCommandACK())) {
				fTracer->TraceCommandError(ret);
				LOG_SIO_CMD_ACK_FAILED();
				break;
			}

			memset(buf, 0, buflen);
			snprintf((char*)buf, buflen - 1, "AtariSIO for Linux" VERSION_STRING "\233"
					"(c) 2003-2008 Matthias Reichl\233");

			fTracer->TraceCommandOK();
			fTracer->TraceApeSpecial(myDriveNo, shortdesc);
			fTracer->TraceDataBlock(buf, buflen, description);
	
			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}

			if ((ret=wrapper->SendDataFrame(buf, buflen))) {
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

			unsigned int buflen = 3;
			uint8_t buf[buflen];
			const char* description = "[ add command ]";

			if ((ret=wrapper->ReceiveDataFrame(buf, buflen)) ) {
				fTracer->TraceCommandError(ret);
				LOG_SIO_RECEIVE_DATA_FAILED();
				break;
			}

			fTracer->TraceCommandOK();
			fTracer->TraceDataBlock(buf, buflen, description);

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
		wrapper->SetBaudrate(ATARISIO_STANDARD_BAUDRATE);
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

bool AtrSIOHandler::SetHighSpeedParameters(uint8_t pokeyDivisor, unsigned int baudrate)
{
	fSpeedByte = pokeyDivisor;
	fHighSpeedBaudrate = baudrate;
	return true;
}
