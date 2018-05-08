#!/bin/sh

set -e

VER=`date '+%y%m%d'`

if [ $# -ge 1 ] ; then
	VER="$1"
fi

NAME=atarisio-$VER

mkdir -p tmp/"${NAME}"
mkdir tmp/"${NAME}"/tools
mkdir tmp/"${NAME}"/tools/6502
mkdir tmp/"${NAME}"/tools/6502/mypdos
mkdir tmp/"${NAME}"/driver
mkdir -p tmp/"${NAME}"/contrib/gentoo
cp -a Changelog* README* INSTALL* LICENSE* Makefile \
	atarisio-modprobe* atarisio-udev* getver.sh getcurses.sh tmp/"${NAME}"
cp -a tools/Makefile* tools/*.cpp tools/*.h tmp/"${NAME}"/tools
cp -a tools/6502/Makefile tools/6502/*.src tools/6502/*.inc \
      tools/6502/*.c tools/6502/*.h tools/6502/*.bin \
      tools/6502/remote.com tmp/"${NAME}"/tools/6502
cp -a tools/6502/mypdos/*.c tmp/"${NAME}"/tools/6502/mypdos
cp -a driver/Makefile driver/*.c driver/*.h tmp/"${NAME}"/driver
cp -a contrib/gentoo/* tmp/"${NAME}"/contrib/gentoo/
mv tmp/"${NAME}"/contrib/gentoo/atarisio.ebuild tmp/"${NAME}"/contrib/gentoo/atarisio-"${VER}".ebuild
cd tmp
tar zcf ../../dist/${NAME}.tar.gz "${NAME}"
cp -a ../Changelog ../README ../../dist
rm -rf "${NAME}"
