#!/bin/sh

# try to get information about curses installation

if [ $# -ne 1 ] ; then
	echo "usage: $0 --cflags|--libs"
	exit 1
fi

CURSES_CONFIG=ncurses5-config

which $CURSES_CONFIG 2>&1 >/dev/null

if [ $? -eq 0 ] ; then
	$CURSES_CONFIG "$@"
else
	case "$1" in
	  "--cflags")
	    ;;
	  "--libs")
	    echo "-L/usr/lib -lncurses"
	    ;;
	  *)
	    exit 1
	    ;;
	esac
fi

