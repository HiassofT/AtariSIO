#!/bin/sh

# get linux version

if [ $# -ne 1 ] ; then
	exit 1
fi

KDIR="$1"

if [ -f "${KDIR}/include/generated/utsrelease.h" ] ; then
	UTSFILE="${KDIR}/include/generated/utsrelease.h"
else
if [ -f "${KDIR}/include/linux/utsrelease.h" ] ; then
	UTSFILE="${KDIR}/include/linux/utsrelease.h"
else
	UTSFILE="${KDIR}/include/linux/version.h"
fi
fi

grep UTS_RELEASE "$UTSFILE" 2>/dev/null | cut -d '"' -f2 | head -1
