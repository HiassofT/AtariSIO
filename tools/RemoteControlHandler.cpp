/*
   RemoteControlHandler - SIO commands for remote control of atariserver

   Copyright (C) 2005 Matthias Reichl <hias@horus.com>

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
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#include "RemoteControlHandler.h"
#include "AtariDebug.h"
#include "MiscUtils.h"
#include "Dos2xUtils.h"
#include "Version.h"
#include "Error.h"
#include "DeviceManager.h"
#include "Coprocess.h"

using namespace MiscUtils;

RemoteControlHandler::RemoteControlHandler(DeviceManager* manager, CursesFrontend* frontend)
	: fDeviceManager(manager),
	  fCursesFrontend(frontend)
{
	if (fDeviceManager == 0) {
		Assert(false);
		throw ErrorObject("RemoteControlHandler needs a DeviceManager pointer");
	}
	if (fCursesFrontend == 0) {
		Assert(false);
		throw ErrorObject("RemoteControlHandler needs a CursesFrontend pointer");
	}
	fTracer = SIOTracer::GetInstance();
	ResetResult();
}

RemoteControlHandler::~RemoteControlHandler()
{
}

int RemoteControlHandler::ProcessCommandFrame(SIO_command_frame& frame, const RCPtr<SIOWrapper>& wrapper)
{
	int ret=0;

	fTracer->TraceCommandFrame(frame);

	switch (frame.command) {
	case 0x43: {
		// execute command

		fLastCommandStatus = eRemoteCommandError;
		int buflen = frame.aux1 + (frame.aux2<<8);
		/*
		if (buflen < 2) {
			if ((ret=wrapper->SendCommandNAK())) {
				LOG_SIO_CMD_NAK_FAILED();
			}
			ret = AbstractSIOHandler::eRemoteControlError;
			fTracer->TraceCommandError(ret);
			ResetResult();
			break;
		}
		*/
		if ((ret=wrapper->SendCommandACK())) {
			LOG_SIO_CMD_ACK_FAILED();
			ResetResult();
			break;
		}


		uint8_t buf[buflen+1];
		const char* description = "[ remote command ]";

		if (buflen) {
			if ((ret=wrapper->ReceiveDataFrame(buf, buflen))) {
				fTracer->TraceCommandError(ret);
				LOG_SIO_RECEIVE_DATA_FAILED();
				ResetResult();
				break;
			}
		}

		buf[buflen]=0;
		ResetResult();

		if (buflen) {
			if (!ProcessMultipleCommands((char*)buf, buflen, wrapper)) {
				ret = AbstractSIOHandler::eRemoteControlError;
				fTracer->TraceCommandError(ret);
	
				fTracer->TraceRemoteControlCommand();
				fTracer->TraceDataBlock(buf, buflen, description);
	
				if (wrapper->SendError()) {
					LOG_SIO_ERROR_FAILED();
				}
				break;
			}
		}

		fTracer->TraceCommandOK();

		fTracer->TraceRemoteControlCommand();
		if (buflen) {
			fTracer->TraceDataBlock(buf, buflen, description);
		}

		if ((ret=wrapper->SendComplete())) {
			LOG_SIO_COMPLETE_FAILED();
			break;
		}
		fLastCommandStatus = eRemoteCommandOK;

		break;
	}
	case 0x53: {
		// get status

		if ((ret=wrapper->SendCommandACK())) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_CMD_ACK_FAILED();
			break;
		}

		unsigned int buflen = 4;
		uint8_t buf[buflen];
		const char* description = "[ get remote control status ]";

		unsigned int resultlen = fResult->GetLength();
		buf[0] = fLastCommandStatus;
		buf[1] = 0;
		buf[2] = resultlen & 0xff;
		buf[3] = resultlen >> 8;

		fTracer->TraceCommandOK();
		fTracer->TraceRemoteControlStatus();
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
	case 0x52: {
		// read result

		unsigned int sec = frame.aux1 + (frame.aux2<<8);
		unsigned int offset = sec * 128;
		unsigned int resultlen = fResult->GetLength();
		if ( offset >= resultlen) {
			if (wrapper->SendCommandNAK()) {
				LOG_SIO_CMD_NAK_FAILED();
			}

			ret = AbstractSIOHandler::eIllegalSectorNumber;

			fTracer->TraceCommandError(ret);
			LOG_SIO_MISC("read RC result: illegal sector number %d",sec);

			break;
		}

		ret = wrapper->SendCommandACK();
		if (ret) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_CMD_ACK_FAILED();
			break;
		}

		unsigned int buflen = 128;
		unsigned int copylen = buflen;
		uint8_t buf[buflen];

		const char* description = "[ read remote control result ]";

		if (offset+buflen>resultlen) {
		       copylen = resultlen - offset;
		       memset(buf+copylen, 0, buflen-copylen);
		}	       

		if (!fResult->GetDataBlock(buf, offset, copylen)) {
			ret = AbstractSIOHandler::eImageError;

			fTracer->TraceCommandError(ret);
			fTracer->TraceReadRemoteControlResult(sec);
			fTracer->TraceDataBlock(buf, buflen, description);

			if (wrapper->SendError()) {
				LOG_SIO_ERROR_FAILED();
				break;
			}
		} else {
			unsigned int i;
			for (i=0;i<copylen;i++) {
				if (!buf[i]) {
					buf[i] = 155;
				}
			}
			fTracer->TraceCommandOK();
			fTracer->TraceReadRemoteControlResult(sec);
			fTracer->TraceDataBlock(buf, buflen, description);

			if ((ret=wrapper->SendComplete())) {
				LOG_SIO_COMPLETE_FAILED();
				break;
			}
		}

		int ret2 = wrapper->SendDataFrame(buf, buflen);
		if (ret2) {
			LOG_SIO_SEND_DATA_FAILED();
			if (ret==0) ret=ret2;
			break;
		}
		break;
	}
	case 0x54: {
		// get time (format identical to ApeTime)

		if ((ret=wrapper->SendCommandACK())) {
			fTracer->TraceCommandError(ret);
			LOG_SIO_CMD_ACK_FAILED();
			break;
		}

		unsigned int buflen = 6;
		uint8_t buf[buflen];
		const char* description = "[ remote control get time ]";
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
		fTracer->TraceRemoteControlGetTime();
		fTracer->TraceDataBlock(buf, buflen, description);

		if ((ret=wrapper->SendComplete())) {
			LOG_SIO_COMPLETE_FAILED();
			break;
		}

		if ((ret=wrapper->SendDataFrame(buf,buflen))) {
			LOG_SIO_SEND_DATA_FAILED();
			break;
		}

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

bool RemoteControlHandler::IsRemoteControlHandler() const
{
	return true;
}

RCPtr<DiskImage> RemoteControlHandler::GetDiskImage()
{
	return 0;
}

RCPtr<const DiskImage> RemoteControlHandler::GetConstDiskImage() const
{
	return 0;
}

bool RemoteControlHandler::EnableHighSpeed(bool)
{
	return false;
}

bool RemoteControlHandler::EnableStrictFormatChecking(bool)
{
	return false;
}

bool RemoteControlHandler::EnableXF551Mode(bool)
{
	return false;
}

bool RemoteControlHandler::SetHighSpeedParameters(unsigned int, unsigned int)
{
	return false;
}

void RemoteControlHandler::ResetResult()
{
	fResult = new DataContainer;
}

void RemoteControlHandler::AddResultString(const char* string)
{
	fResult->AppendString(string);
	fResult->AppendByte(0);
}

bool RemoteControlHandler::ProcessCommand(const char* cmd, const RCPtr<SIOWrapper>& wrapper)
{
	int ret = true;
	const char* arg;
	DeviceManager::EDriveNumber driveno;

	if (!*cmd) {
		Assert(false);
		return false;
	}
	if (*cmd=='?') {
		AddResultString("atariserver remote control help");
		//               1234567890123456789012345678901234567890
		AddResultString("st print status    xc exchange drives");
		AddResultString("lo load drive      un unload drive");
		AddResultString("lv load virtual    rd reload drive");
		AddResultString("wr write drive     wc write changed");
		AddResultString("cr create drive    wp write protect");
		AddResultString("pr printer         ad (de-)activate");
		AddResultString("sp speed           xf XF551 mode");
		AddResultString("cd change dir      ls list directory");
		AddResultString("sh shell command   pl print log");
		return true;
	}

	if (cmd[1]==0) {
		goto unknown_command;
	}

	arg = cmd+2;
	EatSpace(arg);

	if (strncasecmp(cmd,"lo",2)==0) { // load drive
		if (!ValidDriveNo(*arg)) {
			AddResultString("invalid drive number");
			goto lo_usage;
		}
		driveno=GetDriveNo(*arg);
		arg++; EatSpace(arg);
		ret = fDeviceManager->LoadDiskImage(driveno, arg, true, true);
		if (!ret) {
			AddResultString("loading disk image failed");
		}
		fCursesFrontend->DisplayDriveStatus(driveno);
		fCursesFrontend->UpdateScreen();
		return ret;
lo_usage:
		AddResultString("usage: lo <driveno> <filename>");
		return false;
	}

	if (strncasecmp(cmd,"lv",2)==0) { // load virtual drive
		if (!ValidDriveNo(*arg)) {
			AddResultString("invalid drive number");
			goto lv_usage;
		}
		driveno=GetDriveNo(*arg);

		arg++; EatSpace(arg);
		
		switch (*arg) {
		case 's':
			arg++; EatSpace(arg);
			ret = fDeviceManager->CreateVirtualDrive(driveno, arg, e90kDisk, true);
			break;
		case 'e':
			arg++; EatSpace(arg);
			ret = fDeviceManager->CreateVirtualDrive(driveno, arg, e130kDisk, true);
			break;
		case 'd':
			arg++; EatSpace(arg);
			ret = fDeviceManager->CreateVirtualDrive(driveno, arg, e180kDisk, true);
			break;
		case 'S':
			{
				int sectors = Dos2xUtils::EstimateDiskSize(arg, e128BytesPerSector, Dos2xUtils::ePicoName);
				arg++; EatSpace(arg);
				ret = fDeviceManager->CreateVirtualDrive(driveno, arg, e128BytesPerSector, sectors, true);
				break;
			}
		case 'D':
			{
				arg++; EatSpace(arg);
				int sectors = Dos2xUtils::EstimateDiskSize(arg, e256BytesPerSector, Dos2xUtils::ePicoName);
				ret = fDeviceManager->CreateVirtualDrive(driveno, arg, e256BytesPerSector, sectors, true);
				break;
			}
		default:
			{
				arg++; EatSpace(arg);
				char* nextarg;
				unsigned int sectors = strtol(arg, &nextarg, 10);
				if (!nextarg) {
					AddResultString("error in strtol");
					goto lv_usage;
				}
				arg = nextarg;
				if (sectors < 720 || sectors > 65535) {
					AddResultString("invalid number of sectors");
					goto lv_usage;
				}

				ESectorLength seclen;
				switch (toupper(*arg)) {
				case 'S': seclen = e128BytesPerSector; break;
				case 'D': seclen = e256BytesPerSector; break;
				default:
					AddResultString("density not specified");
					goto lv_usage;
				}
				arg++; EatSpace(arg);
				ret = fDeviceManager->CreateVirtualDrive(driveno, arg, seclen, sectors, true);
				if (!ret) {
					AddResultString("error creating virtual drive");
				}
			}
		}
		fCursesFrontend->DisplayDriveStatus(driveno);
		fCursesFrontend->UpdateScreen();
		return ret;
lv_usage:
		AddResultString("usage: lv <driveno> <dens> <directory>");
		AddResultString("<dens> = s|e|d|S|D|<num>s|<num>d");
		AddResultString("num must be 720..65535");
		return false;
	}

	if (strncasecmp(cmd,"rd",2)==0) { // reload drive
		if (*arg == 'a' || *arg == 'A') {
			driveno = DeviceManager::eAllDrives;
		} else {
			if (!ValidDriveNo(*arg) && (*arg != 'a') && (*arg != 'A')) {
				AddResultString("invalid drive number");
				goto rd_usage;
			}
			driveno=GetDriveNo(*arg);
		}

		ret = fDeviceManager->ReloadDrive(driveno);
		if (!ret) {
			AddResultString("error reloading drive(s)");
		}
		fCursesFrontend->DisplayDriveStatus(driveno);
		fCursesFrontend->UpdateScreen();
		return ret;
rd_usage:
		AddResultString("usage: rd <driveno>");
		return false;
	}

	if (strncasecmp(cmd,"cr",2)==0) { // create drive
		if (!ValidDriveNo(*arg)) {
			AddResultString("invalid drive number");
			goto cr_usage;
		}
		driveno=GetDriveNo(*arg);

		arg++; EatSpace(arg);
		
		switch (toupper(*arg)) {
		case 'S':
			ret = fDeviceManager->CreateAtrMemoryImage(driveno, e90kDisk, true);
			break;
		case 'E':
			ret = fDeviceManager->CreateAtrMemoryImage(driveno, e130kDisk, true);
			break;
		case 'D':
			ret = fDeviceManager->CreateAtrMemoryImage(driveno, e180kDisk, true);
			break;
		default:
			{
				char* nextarg;
				unsigned int sectors = strtol(arg, &nextarg, 10);
				if (!nextarg) {
					AddResultString("error in strtol");
					goto cr_usage;
				}
				arg = nextarg;
				if (sectors < 1 || sectors > 65535) {
					AddResultString("illegal number of sectors");
					goto cr_usage;
				}

				ESectorLength seclen;
				switch (toupper(*arg)) {
				case 'S': seclen = e128BytesPerSector; break;
				case 'D': seclen = e256BytesPerSector; break;
				default:
					AddResultString("density not specified");
					goto cr_usage;
				}

				ret = fDeviceManager->CreateAtrMemoryImage(driveno, seclen, sectors, true);
				if (!ret) {
					AddResultString("error creating drive");
				}
			}
		}
		fCursesFrontend->DisplayDriveStatus(driveno);
		fCursesFrontend->UpdateScreen();
		return ret;
cr_usage:
		AddResultString("usage: cr <driveno> <dens>");
		AddResultString("<dens> = s|e|d|S|D|<num>s|<num>d");
		AddResultString("num must be 1..65535");
		return false;
	}

	if (strncasecmp(cmd,"un",2)==0) { // unload drive
		if (toupper(*arg)=='A') {
			driveno=DeviceManager::eAllDrives;
		} else {
			if (!ValidDriveNo(*arg)) {
				AddResultString("invalid drive number");
				goto un_usage;
			}
			driveno=GetDriveNo(*arg);
		}
		fDeviceManager->UnloadDiskImage(driveno);
		fCursesFrontend->DisplayDriveStatus(driveno);
		fCursesFrontend->UpdateScreen();
		return true;
un_usage:
		AddResultString("usage: un a|<driveno>");
		return false;
	}

	if (strncasecmp(cmd,"wr",2)==0) { // write drive
		if (!ValidDriveNo(*arg)) {
			goto wr_usage;
		}
		driveno=GetDriveNo(*arg);
		arg++; EatSpace(arg);
		if (*arg) {
			ret = fDeviceManager->WriteDiskImage(driveno, arg);
		} else {
			ret = fDeviceManager->WriteBackImage(driveno);
		}
		if (!ret) {
			AddResultString("error writing image");
		}
		fCursesFrontend->DisplayDriveStatus(driveno);
		fCursesFrontend->UpdateScreen();
		return ret;
wr_usage:
		AddResultString("usage: wr <driveno> [<filename>]");
		return false;
	}

	if (strncasecmp(cmd,"wc",2)==0) { // write back all changed drives
		ret = fDeviceManager->WriteBackImagesIfChanged();
		if (!ret) {
			AddResultString("error writing back images");
		}
		fCursesFrontend->DisplayDriveStatus();
		fCursesFrontend->UpdateScreen();
		return true;
	}

	if (strncasecmp(cmd,"xc",2)==0) { // exchange drives
		DeviceManager::EDriveNumber driveno2;
		if (!ValidDriveNo(*arg)) {
			AddResultString("invalid first drive number");
			goto xc_usage;
		}
		driveno=GetDriveNo(*arg);
		arg++; EatSpace(arg);
		if (!ValidDriveNo(*arg)) {
			AddResultString("invalid second drive number");
			goto xc_usage;
		}
		driveno2=GetDriveNo(*arg);
		fDeviceManager->ExchangeDrives(driveno, driveno2);
		fCursesFrontend->DisplayDriveStatus(driveno);
		fCursesFrontend->DisplayDriveStatus(driveno2);
		fCursesFrontend->UpdateScreen();
		return true;
xc_usage:
		AddResultString("xc <driveno1> <driveno2>");
		return false;
	}

	if (strncasecmp(cmd,"cd",2)==0) { // change directory
		if (!*arg) {
			goto cd_usage;
		}
		if (chdir(arg)) {
			AddResultString("chdir failed");
			ret = false;
		}
		fCursesFrontend->InitTopLine();
		fCursesFrontend->UpdateScreen();
		return true;
cd_usage:
		AddResultString("usage: cd <directory>");
		return false;
	}

	if (strncasecmp(cmd,"ls",2)==0) { // list directory
		Directory dir;
		int i, size;
		if (*arg) {
			size = dir.ReadDirectory(arg, true);
		} else {
			size = dir.ReadDirectory(".", true);
		}
		if (size < 0) {
			AddResultString("error getting directory listing");
		} else {
			for (i=0;i<size;i++) {
				AddResultString(dir.Get(i)->fName);
			}
		}
		return true;
	}

	if (strncasecmp(cmd,"st",2)==0) { // status
		char cwd[PATH_MAX];
		cwd[0] = 0;
		getcwd(cwd, PATH_MAX-1);
		cwd[PATH_MAX-1] = 0;
		int len = strlen(cwd);
		if (len > 0 && len < PATH_MAX-1 && cwd[len-1] != '/') {
			cwd[len] = '/';
			cwd[len+1] = 0;
		}

		char tmp[100];
		AddResultString("atariserver " VERSION_STRING);
		AddResultString("");
		fResult->AppendString("cwd: ");
		AddResultString(cwd);
		AddResultString("");

		int i;
		for (i=DeviceManager::eMinDriveNumber; i <= DeviceManager::eMaxDriveNumber; i++) {
			DeviceManager::EDriveNumber driveno = DeviceManager::EDriveNumber(i);

			sprintf(tmp,"D%d: ", driveno);
			fResult->AppendString(tmp);
			if (fDeviceManager->DriveInUse(driveno)) {
				char stat_w;
				char stat_c=' ';
				char dens='S';
				int sectors;
				if (fDeviceManager->DeviceIsActive(driveno)) {
					if (fDeviceManager->DriveIsVirtualImage(driveno)) {
						stat_c='V';
					} else {
						if (fDeviceManager->DriveIsChanged(driveno)) {
							stat_c='C';
						}
					}
				} else {
					stat_c = 'I';
				}
				if (fDeviceManager->DriveIsWriteProtected(driveno)) {
					stat_w='P';
				} else {
					stat_w='W';
				}
				if (fDeviceManager->GetConstDiskImage(driveno)->GetSectorLength() == e256BytesPerSector) {
					dens='D';
				}
				sectors=fDeviceManager->GetConstDiskImage(driveno)->GetNumberOfSectors();
				sprintf(tmp,"%5d%c %c%c ", sectors, dens, stat_w, stat_c);
				fResult->AppendString(tmp);

				const char* filename = fDeviceManager->GetImageFilename(driveno);
				if (filename) {
					char *sf = MiscUtils::ShortenFilename(filename, 25);
					AddResultString(sf);
				} else {
					AddResultString("");
				}
			} else {
				AddResultString("------ -- <empty>");
			}
		}
		fResult->AppendString("P1: ");
		if (fDeviceManager->DriveInUse(DeviceManager::ePrinter)) {
			switch (fDeviceManager->GetPrinterEOLConversion()) {
			case PrinterHandler::eRaw:
				fResult->AppendString("   RAW"); break;
			case PrinterHandler::eLF:
				fResult->AppendString("    LF"); break;
			case PrinterHandler::eCR:
				fResult->AppendString("    CR"); break;
			case PrinterHandler::eCRLF:
				fResult->AppendString(" CR+LF"); break;
			}
			fResult->AppendString(" -");
			if (fDeviceManager->DeviceIsActive(DeviceManager::ePrinter)) {
				switch (fDeviceManager->GetPrinterRunningStatus()) {
				case PrinterHandler::eStatusOK:
					fResult->AppendString("  "); break;
				case PrinterHandler::eStatusSpawned:
					fResult->AppendString("S "); break;
				case PrinterHandler::eStatusError:
					fResult->AppendString("E "); break;
				}
			} else {
				fResult->AppendString("I ");
			}
			const char* filename = fDeviceManager->GetPrinterFilename();
			if (filename) {
				char *sf = MiscUtils::ShortenFilename(filename, 25);
				AddResultString(sf);
			} else {
				AddResultString("");
			}
		} else {
			AddResultString("------ -- <empty>");
		}

		AddResultString("");
		fResult->AppendString("speed: ");
		if (fDeviceManager->GetHighSpeedMode()) {
			fResult->AppendString("high");
		} else {
			fResult->AppendString("low");
		}
		fResult->AppendString("  XF551: ");
		if (fDeviceManager->GetXF551Mode()) {
			AddResultString("on");
		} else {
			AddResultString("off");
		}

		return true;
	}
	if (strncasecmp(cmd,"wp",2)==0) { // set write (un-)protect
		if (!ValidDriveNo(*arg)) {
			AddResultString("invalid drive number");
			goto wp_usage;
		}
		driveno=GetDriveNo(*arg);
		if (!fDeviceManager->DriveInUse(driveno)) {
			AddResultString("drive is empty");
			return false;
		}
		arg++; EatSpace(arg);
		bool prot;
		switch (*arg) {
		case '0': prot = false; break;
		case '1': prot = true; break;
		default:
			AddResultString("wrong write protect status");
			goto wp_usage;
		}
		ret = fDeviceManager->SetWriteProtectImage(driveno, prot);
		if (!ret) {
			AddResultString("write protect failed");
		}
		fCursesFrontend->DisplayDriveStatus(driveno);
		fCursesFrontend->UpdateScreen();
		return ret;
wp_usage:
		AddResultString("usage: wp <driveno> 0|1");
		return false;
	}
	if (strncasecmp(cmd,"ad",2)==0) { // (de-)activate device
		switch (*arg) {
		case 'p':
		case 'P':
			driveno = DeviceManager::ePrinter;
			if (!fDeviceManager->DriveInUse(driveno)) {
				AddResultString("no printer handler installed");
				return false;
			}
			break;
/*
		case 'r':
		case 'R':
			driveno = DeviceManager::eRemoteControl;
			break;
*/
		case 'a':
		case 'A':
			driveno = DeviceManager::eAllDrives;
			break;
		default:
			if (!ValidDriveNo(*arg)) {
				AddResultString("invalid drive number");
				goto ad_usage;
			}
			driveno=GetDriveNo(*arg);
			if (!fDeviceManager->DriveInUse(driveno)) {
				AddResultString("drive is empty");
				return false;
			}
			break;
		}
		arg++; EatSpace(arg);
		bool act;
		switch (*arg) {
		case '0': act = false; break;
		case '1': act = true; break;
		default:
			AddResultString("wrong active status");
			goto ad_usage;
		}
		ret = fDeviceManager->SetDeviceActive(driveno, act);
		if (!ret) {
			AddResultString("device activation failed");
		}
		switch (driveno) {
		case DeviceManager::ePrinter:
			fCursesFrontend->DisplayPrinterStatus();
			break;
		case DeviceManager::eRemoteControl:
			fCursesFrontend->DisplayStatusLine();
			break;
		default:
			fCursesFrontend->DisplayDriveStatus(driveno);
		}
		fCursesFrontend->UpdateScreen();
		return ret;
ad_usage:
//		AddResultString("usage: ad <driveno>|p|r|a 0|1");
		AddResultString("usage: ad <driveno>|a|p 0|1");
		return false;
	}
	if (strncasecmp(cmd,"sp",2)==0) { // set high speed mode
		bool high;
		switch (*arg) {
		case '0': high = false; break;
		case '1': high = true; break;
		default:
			AddResultString("invalid speed mode");
			goto sp_usage;
		}
		ret = fDeviceManager->SetHighSpeedMode(high);
		if (!ret) {
			AddResultString("setting speed failed");
		}
		fCursesFrontend->DisplayStatusLine();
		fCursesFrontend->UpdateScreen();
		return ret;
sp_usage:
		AddResultString("usage: sp 0|1");
		return false;
	}
	if (strncasecmp(cmd,"xf",2)==0) { // set XF551 mode
		bool xf;
		switch (*arg) {
		case '0': xf = false; break;
		case '1': xf = true; break;
		default:
			AddResultString("invalid XF551 mode");
			goto xf_usage;
		}
		ret = fDeviceManager->EnableXF551Mode(xf);
		if (!ret) {
			AddResultString("setting XF551 mode failed");
		}
		fCursesFrontend->DisplayStatusLine();
		fCursesFrontend->UpdateScreen();
		return ret;
xf_usage:
		AddResultString("usage: xf 0|1");
		return false;
	}
	if (strncasecmp(cmd,"pr",2)==0) { // (un-)install printer handler
		switch (*arg) {
		case '0': {
				if (!fDeviceManager->DriveInUse(DeviceManager::ePrinter)) {
					AddResultString("no printer handler installed");
					return false;
				}
				ret = fDeviceManager->RemovePrinterHandler();
				if (!ret) {
					AddResultString("removing printer handler failed");
				}
			}
			break;
		case '1': {
				arg++; EatSpace(arg);
				PrinterHandler::EEOLConversion eol;
				switch (*arg) {
				case 'r':
				case 'R':
				       	eol = PrinterHandler::eRaw; break;
				case 'l':
				case 'L':
					eol = PrinterHandler::eLF; break;
				case 'c':
				case 'C':
					eol = PrinterHandler::eCR; break;
				case 'b':
				case 'B':
					eol = PrinterHandler::eCRLF; break;
					eol = PrinterHandler::eCRLF; break;
				default:
					  AddResultString("illegal eol mode");
					  goto pr_usage;
				}
				arg++; EatSpace(arg);
				ret = fDeviceManager->InstallPrinterHandler(arg, eol);
				if (!ret) {
					AddResultString("installing printer handler failed");
				}
			}
			break;
		case '2':
		case 'f':
		case 'F':
			  if (!fDeviceManager->DriveInUse(DeviceManager::ePrinter)) {
				  AddResultString("no printer handler installed");
				  return false;
			  }
			  ret = fDeviceManager->FlushPrinterData();
			  if (!ret) {
				  AddResultString("flushing printer data failed");
			  }
			  break;
		default:
			AddResultString("invalid printer mode");
			goto pr_usage;
		}
		fCursesFrontend->DisplayPrinterStatus();
		fCursesFrontend->UpdateScreen();
		return ret;
pr_usage:
		AddResultString("usage: pr 0|1 (r|l|c|b) <filename>|f");
		return false;
	}
	if (strncasecmp(cmd,"sh",2)==0) { // shell command
		if (*arg) {
			Coprocess *coprocess = NULL;
			unsigned int buflen=1024;
			char buf[buflen];
			int len;
			try {
				int fd = wrapper->GetDeviceFD();
				coprocess = new Coprocess(arg, &fd, 1);
			}
			catch (ErrorObject& err) {
				AddResultString(err.AsCString());
				return false;
			}
			while (coprocess->ReadLine(buf, buflen, len, false)) {
				ALOG("response: %s", buf);
				AddResultString(buf);
			}
			if (!coprocess->Close()) {
				AERROR("closing remote control shell command failed");
				AddResultString("error closing shell command");
			}
			coprocess->SetKillTimer(5);
			while (coprocess->ReadLine(buf, buflen, len, true)) {
				AddResultString(buf);
				
			}
			int stat;
			if (!coprocess->Exit(&stat)) {
				AERROR("exiting remote control shell command failed");
				AddResultString("error exiting shell command");
			}
			coprocess->SetKillTimer(0);
			if (WEXITSTATUS(stat)) {
				snprintf(buf, buflen, "command returned %d", WEXITSTATUS(stat));
				// AddResultString(buf);
				AWARN("remote command: %s", buf);
			}
			if (WIFSIGNALED(stat)) {
				int sig = WTERMSIG(stat);
				if (sig == SIGKILL) {
					snprintf(buf, buflen, "program killed!");
				} else {
					snprintf(buf, buflen, "program terminated by signal %d!", sig);
				}
				AddResultString(buf);
				AWARN("remote command: %s", buf);
			}
			if (WIFSTOPPED(stat)) {
				int sig = WSTOPSIG(stat);
				if (sig == SIGKILL) {
					snprintf(buf, buflen, "program killed!");
				} else {
					snprintf(buf, buflen, "program stopped by signal %d!", sig);
				}
				AddResultString(buf);
				AWARN("remote command: %s", buf);
			}
			delete coprocess;
		} else {
			AddResultString("usage: sh command...");
			return false;
		}
		return true;
	}

	if (strncasecmp(cmd,"pl",2)==0) { // print log string
		ALOG("[rc log]: %s", arg);
		return true;
	}

unknown_command:
	AddResultString("unknown command - use ? for help");

	return false;
}

bool RemoteControlHandler::ProcessMultipleCommands(const char* buf, int buflen, const RCPtr<SIOWrapper>& wrapper)
{
	int ok = true;
	int len;
	int pos = 0;
	const char* cmd;
	while (ok && pos < buflen) {
		cmd = buf + pos;
		len = strlen(cmd);
		if (len) {
			ok = ProcessCommand(cmd, wrapper);
		}
		pos = pos + len + 1;
	}
	return ok;
}

