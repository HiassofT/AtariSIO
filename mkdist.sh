#!/bin/sh

VER=`date '+%y%m%d'`

if [ $# -ge 1 ] ; then
	VER="$1"
fi

NAME=atarisio-$VER

mkdir -p tmp/"${NAME}"
mkdir tmp/"${NAME}"/tools
mkdir tmp/"${NAME}"/tools/6502
mkdir tmp/"${NAME}"/driver
cp -a Changelog* README* INSTALL* LICENSE* Makefile atarisio-ttyS* getver.sh getcurses.sh tmp/"${NAME}"
cp -a tools/Makefile tools/*.cpp tools/*.h tmp/"${NAME}"/tools
cp -a tools/6502/Makefile tools/6502/*.src tools/6502/*.inc \
      tools/6502/*.c tools/6502/*.h tools/6502/*.bin \
      tools/6502/remote.com tmp/"${NAME}"/tools/6502
cp -a driver/Makefile driver/*.c driver/*.h tmp/"${NAME}"/driver
cd tmp
tar zcf ../dist/${NAME}.tar.gz "${NAME}"
cp -a ../Changelog ../README ../dist
rm -rf "${NAME}"
