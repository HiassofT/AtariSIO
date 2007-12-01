/*
   Error.cpp - error exception objects

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

#include "Error.h"

#include <string.h>

ErrorObject::ErrorObject()
	: fDescription(0)
{ }

ErrorObject::ErrorObject(const char* string)
	: fDescription(0)
{
	SetDescription(string);
}

ErrorObject::ErrorObject(const ErrorObject& other)
	: fDescription(0)
{
	SetDescription(other.fDescription);
}

ErrorObject::~ErrorObject()
{
	if (fDescription) {
		delete [] fDescription;
	}
}

const char* ErrorObject::AsString() const
{
	if (fDescription) {
		return fDescription;
	} else {
		return "unknown error";
	}
}

void ErrorObject::SetDescription(const char* string)
{
	if (fDescription) {
		delete [] fDescription;
		fDescription = 0;
	}
	if (string) {
		fDescription = new char[strlen(string)+1];
		strcpy(fDescription, string);
	}
}

void ErrorObject::SetDescription(const char* str1, const char* str2)
{
	if (fDescription) {
		delete [] fDescription;
		fDescription = 0;
	}
	if (str1 || str2) {
		unsigned int l1 = 0;
		unsigned int l2 = 0;
		if (str1) {
			l1 = strlen(str1);
		}
		if (str2) {
			l2 = strlen(str2);
		}
		fDescription = new char[l1+l2+1];
		fDescription[l1+l2] = 0;
		if (l1) {
			strcpy(fDescription, str1);
		}
		if (l2) {
			strcpy(fDescription + l1, str2);
		}
	}
}

