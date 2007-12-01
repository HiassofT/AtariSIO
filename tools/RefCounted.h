#ifndef REFCOUNTED_H
#define REFCOUNTED_H

/*
   RefCounted.h - base class providing refcounting of objects

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

class RefCounted {
public:
	RefCounted();

	RefCounted(const RefCounted& other);

	virtual ~RefCounted() {}

	inline void Ref() const;
	inline void UnRef() const;

	bool IsShared() const;

	int GetRefCount() const;

protected:
	RefCounted& operator=(const RefCounted& other);

private:
	mutable int refCount;
};

inline RefCounted::RefCounted()
	: refCount(0)
{}

// don't copy refcount!
inline RefCounted::RefCounted(const RefCounted&)
	: refCount(0)
{}

inline void RefCounted::Ref() const
{
	refCount++;
}

inline void RefCounted::UnRef() const
{
	refCount--;
	if (refCount == 0) {
		delete this;
	}
}

inline bool RefCounted::IsShared() const
{
	return refCount > 1;
}

inline int RefCounted::GetRefCount() const
{
	return refCount;
}

inline RefCounted& RefCounted::operator=(const RefCounted&)
{
	return *this;
}

#endif
