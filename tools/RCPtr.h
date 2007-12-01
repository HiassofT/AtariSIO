#ifndef RCPTR_H
#define RCPTR_H

/*
   RCPtr.h - a ref-counted smart pointer template

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

#include <assert.h>

template<class T>
class RCPtr {
	template <class T2> friend class RCPtr;
public:

	inline RCPtr(T* realPtr = 0);
	inline RCPtr(const RCPtr<T>& other);
        template<class T2>
		inline RCPtr(const RCPtr<T2>& other);

	inline ~RCPtr();

	inline RCPtr<T>& operator=(const RCPtr<T>& other);
        template<class T2>
		inline RCPtr<T>& operator=(const RCPtr<T2>& other);
	inline RCPtr<T>& operator=(T *const otherPtr);

	inline T* operator->() const;
	inline T& operator*() const;

	inline bool operator==(const RCPtr<T>& other) const;
	inline bool operator!=(const RCPtr<T>& other) const;

	// conversion to bool, for use in expressions like
	// if (myptr) or if (!myptr)
	inline operator bool() const;
	inline bool operator!() const;

	inline bool IsNull() const;
	inline bool IsNotNull() const;

	// this is the same as operator=(0)
	inline void SetToNull();

	// get internal pointer
	// use with care! The refcount is not incremented by this
	// method, so you might end up with a dangling pointer if
	// the object is destroyed some time after this call!

	inline T* GetRealPointer() const;

private:
	T* pointee;
};

template<class T>
inline RCPtr<T>::RCPtr(T* realPtr)
	: pointee(realPtr)
{
	if (pointee) {
		pointee->Ref();
	}
}

template<class T>
inline RCPtr<T>::RCPtr(const RCPtr& other)
	: pointee(other.pointee)
{
	if (pointee) {
		pointee->Ref();
	}
}

template<class T> template<class T2>
inline RCPtr<T>::RCPtr(const RCPtr<T2>& other)
	: pointee(other.pointee)
{
	if (pointee) {
		pointee->Ref();
	}
}


template<class T>
inline RCPtr<T>::~RCPtr()
{
	if (pointee) {
		pointee->UnRef();
	}
}

template<class T>
inline RCPtr<T>& RCPtr<T>::operator=(const RCPtr<T>& other)
{
	if (pointee != other.pointee) {
		if (pointee != 0) {
			pointee->UnRef();
		}
		pointee = other.pointee;
		if (pointee != 0) {
			pointee->Ref();
		}
	}
	return *this;
}

template<class T> template<class T2>
inline RCPtr<T>& RCPtr<T>::operator=(const RCPtr<T2>& other)
{
	if (pointee != other.pointee) {
		if (pointee != 0) {
			pointee->UnRef();
		}
		pointee = other.pointee;
		if (pointee != 0) {
			pointee->Ref();
		}
	}
	return *this;
}

template<class T>
inline bool RCPtr<T>::operator==(const RCPtr<T>& other) const
{
	return pointee == other.pointee;
}

template<class T>
inline bool RCPtr<T>::operator!=(const RCPtr<T>& other) const
{
	return !operator==(other);
}

template<class T>
RCPtr<T>& RCPtr<T>::operator=(T* const otherPtr)
{
	if (pointee != otherPtr) {
		if (pointee != 0) {
			pointee->UnRef();
		}
		pointee = otherPtr;
		if (pointee != 0) {
			pointee->Ref();
		}
	}
	return *this;
}

template<class T>
inline T* RCPtr<T>::operator->() const
{
	assert(pointee && pointee->GetRefCount()>=0);
	return pointee;
}

template<class T>
inline T& RCPtr<T>::operator*() const
{
	return *pointee;
}

template<class T>
inline RCPtr<T>::operator bool() const
{
	return pointee != 0;
}

template<class T>
inline bool RCPtr<T>::IsNull() const
{
	return pointee == 0;
}

template<class T>
inline bool RCPtr<T>::IsNotNull() const
{
	return pointee != 0;
}

template<class T>
inline bool RCPtr<T>::operator!() const
{
	return pointee == 0;
}

template<class T>
inline void RCPtr<T>::SetToNull()
{
	if (pointee) {
		pointee->UnRef();
	}
	pointee = 0;
}

template<class T>
inline T* RCPtr<T>::GetRealPointer() const
{
	return pointee;
}

// type casts
template<class T2, class T> inline
	RCPtr<T2> RCPtrStaticCast(const RCPtr<T>& ptr)
{
	return RCPtr<T2>(static_cast<T2*>(ptr.GetRealPointer()));
}

template<class T2, class T> inline
	RCPtr<T2> RCPtrDynamicCast(const RCPtr<T>& ptr)
{
	return RCPtr<T2>(dynamic_cast<T2*>(ptr.GetRealPointer()));
}

template<class T2, class T> inline
	RCPtr<T2> RCPtrConstCast(const RCPtr<T>& ptr)
{
	return RCPtr<T2>(const_cast<T2*>(ptr.GetRealPointer()));
}

#endif
