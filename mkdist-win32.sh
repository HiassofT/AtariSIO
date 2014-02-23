#!/bin/sh

VER=`date '+%y%m%d'`

if [ $# -ge 1 ] ; then
	VER="$1"
fi

NAME=tools-win32-$VER

mkdir -p tmp/"${NAME}"

cp -a README-tools tmp/"${NAME}"/README.txt
todos tmp/"${NAME}"/README.txt

cp -a LICENSE tmp/"${NAME}"
todos tmp/"${NAME}"/LICENSE

cp -a tools/adir.exe tools/ataricom.exe tools/dir2atr.exe tmp/"${NAME}"

cd tmp/"${NAME}"
zip ../../dist/${NAME}.zip *

cd ..
rm -rf "${NAME}"
cp -a ../README-tools ../dist
