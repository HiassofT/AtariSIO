AtariSIO tools V0.30

Copyright (C) 2003-2022 Matthias Reichl <hias@horus.com>

This program is proteced under the terms of the GNU General Public
License, version 2. Please read LICENSE for further details.

Visit http://www.horus.com/~hias/atari/ for new versions.


adir
====

'adir' lists the DOS 2.x directory of an image. By default the output
is similar to what you get when doing a directory listing from within
DOS 2.x. Filenames plus their sector size is output in 4 columns.

Usage: adir [option] file.atr...

Option may be one of:

-<num> set number of columns. eg: 'adir -1 dos.atr'

-t  enable 'tree mode', recurse into (MyDOS) subdirectories

-r  enable raw debugging output:
    The first number is the file-slot within a directory sector (0-7).
    The second number is the file attribute (in hex).
    After the filename the starting sector and the number of sectors
    (both in decimal) are shown.

Note: you can also pass multiple ATR files to adir, for example
"adir file1.atr file2.atr file3.atr". In Linux you can also use
the pathname expansion feature of your shell and get a listing
of all files: "adir *.atr".

  
dir2atr
=======

With 'dir2atr' you can create a disk image from a directory on your PC.

Usage: dir2atr [options] [size] file.atr directory

If you don't specify the size (in sectors), dir2atr will automatically
calculate it so that all files in the directory will fit into the
image.

To create an image that will fit on a physical disk use one of the
'-S', '-E', '-D' or '-Q' options. These options set both the density
and number of sectors to match the standard SD (90k), ED (130k),
DD (180k) and QD (360k) sizes.

If you set the image size to 720 sectors (either SD or DD) or to 1040
SD sectors (the 1050 "enhanced" density format), the disk image
will be created using standard DOS 2.x format, otherwise dir2atr
creates an image in MyDOS format.

Note: in MyDOS format dir2atr also descends into subdirectories.

Options:

-d  create a double density (256 bytes per sector) image
    (default is single density, 128 byte per sector).

-m  create a MyDOS image (use this option to create a 720 or
    1040 sectors image with subdirectories).

-S  create a standard SD/90k image

-E  create a standard ED/130k image

-D  create a standard DD/180k image

-Q  create a standard QD/360k image in MyDOS format

-p  create a 'PICONAME.TXT' (long filenames for MyPicoDos) in
    each (sub-) directory of the image.

-P  like '-p', but the extension of is stripped from the long
    names in PICONAME.TXT. So, for example a file 'Boulder Dash.com'
    on your PC will show up as 'Boulder Dash' in MyPicoDos.

-a  enable MyPicoDos 4.04/4.05/4.06 autorun mode
    Note: only a single file may be present on disk
    (in addition to PICODOS.SYS and, optionally, PICONAME.TXT)
    which will then be automatically loaded on boot.

-b <dostype> create a bootable image. 

-B <filename> load boot sector data (384 bytes) from file.

Bootable Images:

If you use one of the MyPicoDos modes, dir2atr will include the
'PICODOS.SYS' file in the disk image and also write the bootsectors.
This is more or less identical to initializing the disk using the
'myinit.com' program on your Atari.

A few other DOSes are supported in '-b', too, but dir2atr only
writes the bootsectors, you have to provide the DOS.SYS (and
optionally DUP.SYS) by yourself. Be careful to use the correct
DOS.SYS file, otherwise the disk won't work or the DOS might
behave strangely. dir2atr only checks for the presence of DOS.SYS
but not if it's the correct version!

Supported DOSes:
Dos20           DOS 2.0
Dos25           DOS 2.5
OSAplus210      OSS OS/A+ 2.10
DosXL230        OSS DOS XL 2.30
MyDos4533       MYDOS 4.53/3
MyDos4534       MYDOS 4.53/4
MyDos455Beta4   MYDOS 4.55 Beta 4
TurboDos21      Turbo Dos 2.1
TurboDos21HS    Turbo Dos 2.1 HS (ultra-speed support)
XDos243F        XDOS 2.43F ("fast" version / ultra-speed support)
XDos243N        XDOS 2.43N ("normal speed" version)

Supported MyPicoDos variants:
Currently the old 4.03, 4.04, 4.05 and the current 4.0 version of MyPicoDos
are supported, in several different configurations. I recommend using
version 4.06 because it comes with improved highspeed SIO support.

MyPicoDos 4.03 variants:
MyPicoDos403    standard SIO speed only
MyPicoDos403HS  highspeed SIO support

MyPicoDos 4.04 variants:
These versions (except for the barebone version) all include
highspeed SIO (HS) support plus optionally an atariserver remote
console (RC). Since most Atari emulators react allergic to the
highspeed SIO code, and the highspeed SIO also has to be disabled
to use MyPicoDos with (PBI) harddrives, there exist versions that have
highspeed SIO disabled by default (but it can be enabled manually
from within MyPicoDos):

MyPicoDos404    HS default: on   remote console: no
MyPicoDos404N   HS default: off  remote console: no
MyPicoDos404R   HS default: on   remote console: yes
MyPicoDos404RN  HS default: off  remote console: yes
MyPicoDos404B   barebone version: no highspeed SIO and no remote console

MyPicoDos 4.05 variants:
This version introduces an improved highspeed SIO code (up to 126kbit/sec,
Pokey divisor 0) and also speeds up loading of MyPicoDos by activating
the highspeed SIO code during the boot process. In case of an transmission
error the highspeed code is automatically switched off, so compatibility
with emulators is slightly improved. For full compatibility you can
also choose highspeed SIO to be automatically activated after booting
('A' versions) or be in a default 'off' ('N' versions).

4.05 also adds special version for SDrive users: after booting
MyPicoDos sends a special command to the SDrive to switch it
to Pokey divisor 0 (126kbit/sec) or 1 (110kbit/sec).

Then there's also the new 'PicoBoot' boot. This is an extremely
stripped down version that fits into the 3 boot sectors and loads
the first (COM/EXE/XEX) file on the disk. No other features
(menu, highspeed SIO, disabling basic, support for BIN/BAS, ...)
are supported.

Like in 4.04 there are also version which include the atariserver
remote console ('R' versions).

MyPicoDos405    HS enabled while booting   remote console: no
MyPicoDos405A   HS default: on after boot  remote console: no
MyPicoDos405N   HS default: off            remote console: no
MyPicoDos405R   HS enabled while booting   remote console: yes
MyPicoDos405RA  HS default: on after boot  remote console: yes
MyPicoDos405RN  HS default: off            remote console: yes
MyPicoDos405S0  Like 405, set SDrive to Pokey divisor 0
MyPicoDos405S1  Like 405, set SDrive to Pokey divisor 1
MyPicoDos405B   barebone version: no highspeed SIO, no remote console
PicoBoot405     minimal boot-sector only version

MyPicoDos 4.06 variants:
This version comes with improved joystick support and fixes a few
minor issues present in 4.05. The variants are the same as for
4.05:

MyPicoDos406    HS enabled while booting   remote console: no
MyPicoDos406A   HS default: on after boot  remote console: no
MyPicoDos406N   HS default: off            remote console: no
MyPicoDos406R   HS enabled while booting   remote console: yes
MyPicoDos406RA  HS default: on after boot  remote console: yes
MyPicoDos406RN  HS default: off            remote console: yes
MyPicoDos406S0  Like 406, set SDrive to Pokey divisor 0
MyPicoDos406S1  Like 406, set SDrive to Pokey divisor 1
MyPicoDos406B   barebone version: no highspeed SIO, no remote console
PicoBoot406     minimal boot-sector only version

User defined boot sectors:

The '-B' option can be used to load the boot sector contents
from a file. The first 384 bytes from the file are written to
the 3 boot sectors.

If the '-b' and '-B' options are used at the same time, '-B' has
priority.

Examples:

dir2atr -d -b MyPicoDos405 -P games.atr my_games_dir

This creates a double-density MyDOS image named 'games.atr' from
the directory 'my_games_dir' (including subdirectories) with
MyPicoDos 4.05 (highspeed SIO enabled during booting) and with
MyPicoDos long filenames ('PICONAME.TXT').

dir2atr -B xboot256.obx -d 720 xbios_test.atr xbios_dir

This creates a 180k DD disk with the xBIOS boot sectors loaded
from xboot256.obx in the current directory.


ataricom
========

ataricom is kind of a swiss army knife for Atari (COM/EXE/XEX)
executable files.

Usage: ataricom [options] infile [outfile]

By default ataricom prints the blocks in the file:
The sequential block number (starting from 1), starting and
ending address of the block (in hex), the number of bytes in
this block and the file-offset to the data part of the block are
printed (in decimal).

If the block includes the addresses $02E0-$02E1 ("RUN") or
$02E2-$02E3 ("INIT") the run/init address is printed,
too (also in hex).

The output file is optional in this "print only" mode, if 
specified a copy of the input file will be created. This file
is identical to the input file, only additional "$FF, $FF" block
within the input file will be skipped.

All other modes ("create", "process", "extract") require an output file.

Options:

-c address      create a COM file from the raw 'infile' data starting
                at the specified address. Options '-r' and '-i' may
                also be used together with this option.

-e              extract blocks into separate files with 4-digit decimal
                block number and the extension ".obj" or ".bin" (if raw mode
                is selected) appended.

-E              same as above but in addition to the block number also
                the hexadecimal start and end address are included in the
                filename.

-r address      add a "RUN" ($02E0/$02E1) block with specified address
                at the end of the file. Can not be used together with '-n'.

-i address      add an "INIT" ($02E2/$02E3) block with specified address
                at the end of the file. Can not be used together with '-n'.

-b range[,...]  Only process the specified block numbers.
                range may be a single block number or a range in the form
                "start-end". The end block number is optional, eg "start-"
                means all blocks from start up to the last one.
                Multiple ranges can be specified, separated by a comma, or
                by using this option multiple times.
                Note: this option can not be used together with '-x'.

-x range[,...]  Process all blocks except the specified block numbers.
                range syntax is the same as in the '-b' option.
                Note: this option can not be used together with '-b'.

-m range[,...]  Merge the data parts of the blocks into a single block.
		range has to be specified in the form "start-end", end
                may be omitted but contrary to '-b' or '-x' options single
                block numbers are not allowed.

-s block,adr... Split a block into multiple blocks at the specified
                address(es). Multiple addresses may be given in the
                form adr1,adr2,adr3...

-n              Write RAW data to outfile, i.e. skip the COM header(s).
                Mostly useful when used with a single input COM block.

-X              Print the block lengths and file offsets in hex instead
                of decimal.

Note: addresses and block numbers may be written in decimal or
in hex (either prefixed with '$' or '0x').

Examples:

ataricom -b 2 -n in.com out.raw
write the data part of block 2 to out.raw.

ataricom -x 1-10 -x 12-20 in.com out.com
ataricom -x 1-10,12-20 in.com out.com
ataricom -b 11 -b 21- in.com out.com
ataricom -b 11,21- in.com out.com
write all blocks except 1-10 and 12-20 (i.e. only blocks 11 and 21 upwards)
to out.com.

ataricom -x 5-6 -m 1-10 in.com out.com
merge block 1-4 and 7-10 of into a single data block and copy blocks 11+.

ataricom -s 1,0x1000,0x2000 -r 0xA000 in.com out.com
Split block 1 at adress 0x1000 and 0x2000: this creates 3 blocks:
the first from the beginning of block 1 up to $0FFF, then one from
$1000-$1FFF and one from $2000 to the end of the first block.
Additionaly a "RUN $A000" block is added to the end of the file.

ataricom -E input.com out
Extract the blocks into separate files named outBBBB_SSSS_EEEE.obj
