#ifndef ERROR_H
#define ERROR_H

/*
   Error.h - error exception objects

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

#include <string>

class ErrorObject {
public:
	ErrorObject();
	ErrorObject(const ErrorObject& );
	ErrorObject(const std::string description);

	virtual ~ErrorObject();

	inline const std::string& AsString() const;
	inline const char* AsCString() const;

protected:
	void SetDescription(const std::string str);

private:
	std::string fDescription;
};

inline const std::string& ErrorObject::AsString() const
{
	return fDescription;
}

inline const char* ErrorObject::AsCString() const
{
	return fDescription.c_str();
}


class FileOpenError : public ErrorObject {
public:
	FileOpenError(const std::string filename)
		: ErrorObject("error opening " + filename)
	{ }
	virtual ~FileOpenError() {}
};

class FileCreateError : public ErrorObject {
public:
	FileCreateError(const std::string filename)
		: ErrorObject("error creating " + filename)
	{ }
	virtual ~FileCreateError() {}
};

class FileReadError : public ErrorObject {
public:
	FileReadError(const std::string filename)
		: ErrorObject("error reading " + filename)
	{ }
	virtual ~FileReadError() {}
};

class FileWriteError : public ErrorObject {
public:
	FileWriteError(const std::string filename)
		: ErrorObject("error writing " + filename)
	{ }
	virtual ~FileWriteError() {}
};

// general I/O errors, without a filename

class EOFError : public ErrorObject {
public:
	EOFError()
		: ErrorObject("Got EOF")
	{ }
	virtual ~EOFError() {}
};

class ReadError : public ErrorObject {
public:
	ReadError()
		: ErrorObject("read error")
	{ }
	virtual ~ReadError() {}
};

class DeviceInitError : public ErrorObject {
public:
	DeviceInitError(const std::string cause)
		: ErrorObject("error initializing device: " + cause)
	{ }
	virtual ~DeviceInitError() {}
};

#endif
