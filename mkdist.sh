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
mkdir -p tmp/"${NAME}"/contrib/lotharek
mkdir -p tmp/"${NAME}"/contrib/rpi
cp -a Changelog* README* INSTALL* LICENSE* Makefile \
	atarisio-modprobe* atarisio-udev* getver.sh getcurses.sh tmp/"${NAME}"
cp -a tools/Makefile* tools/*.cpp tools/*.h tmp/"${NAME}"/tools
cp -a tools/6502/Makefile tools/6502/*.src tools/6502/*.inc \
      tools/6502/*.c tools/6502/*.h tools/6502/*.bin \
      tools/6502/remote.com tmp/"${NAME}"/tools/6502
cp -a tools/6502/mypdos/*.c tmp/"${NAME}"/tools/6502/mypdos
cp -a driver/Makefile driver/atarisio.c driver/atarisio.h tmp/"${NAME}"/driver
cp -a contrib/lotharek/Makefile contrib/lotharek/*.sh \
      contrib/lotharek/*.c tmp/"${NAME}"/contrib/lotharek
cp -a contrib/rpi/Makefile contrib/rpi/*.dts contrib/rpi/*.dtbo \
      tmp/"${NAME}"/contrib/rpi
cd tmp
tar zcf ../../dist/${NAME}.tar.gz "${NAME}"
cp -a ../Changelog ../README ../../dist
rm -rf "${NAME}"
