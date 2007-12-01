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

class ErrorObject {
public:
	ErrorObject();
	ErrorObject(const ErrorObject& );
	ErrorObject(const char* description);

	virtual ~ErrorObject();

	const char* AsString() const;

protected:
	void SetDescription(const char* string);
	void SetDescription(const char* str1, const char* str2);

private:
	char* fDescription;
};

class FileOpenError : public ErrorObject {
public:
	FileOpenError(const char* filename)
		: ErrorObject()
	{ SetDescription("error opening ", filename); }
	virtual ~FileOpenError() {}
};

class FileCreateError : public ErrorObject {
public:
	FileCreateError(const char* filename)
		: ErrorObject()
	{ SetDescription("error creating ", filename); }
	virtual ~FileCreateError() {}
};

class FileReadError : public ErrorObject {
public:
	FileReadError(const char* filename)
		: ErrorObject()
	{ SetDescription("error reading ", filename); }
	virtual ~FileReadError() {}
};

class FileWriteError : public ErrorObject {
public:
	FileWriteError(const char* filename)
		: ErrorObject()
	{ SetDescription("error writing ", filename); }
	virtual ~FileWriteError() {}
};

#endif
