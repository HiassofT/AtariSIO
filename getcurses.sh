#!/bin/sh

# try to get information about curses installation

if [ $# -ne 1 ] ; then
	echo "usage: $0 --cflags|--libs"
	exit 1
fi

CFLAGS=$(pkg-config $1 ncurses panel 2>/dev/null)
if [ $? -eq 0 ] ; then
	echo "$CFLAGS"
	exit 0
fi

CFLAGS=$(pkg-config $1 ncursesw panelw 2>/dev/null)
if [ $? -eq 0 ] ; then
	echo "$CFLAGS"
	exit 0
fi

case "$1" in
  "--cflags")
    ;;
  "--libs")
    echo "-L/usr/lib -lncurses -lpanel"
    ;;
  *)
    exit 1
    ;;
esac
