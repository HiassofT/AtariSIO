ATASM=atasm
#ATASM=/data/src/xl/atasm103/src/atasm

CFLAGS=-W -Wall

all: atarisio-highsio.c mypicodoscode.c remote.com
# speedycode.c

#all: highspeedcode.c mypicodoscode.c MYPDOS.COM mypdos.atr mypdoshs.atr \
#     speedycode.c

#all: MYPDOS.COM mypdos.atr mypdoshs.atr myinit2.atr

#ASMFLAGS=
ASMFLAGS = -s

bootloader.bin: bootloader.src
	$(ATASM) -r -f0 -obootloader.bin bootloader.src

dumpmypdoscode: dumpmypdoscode.o
	$(CC) -o dumpmypdoscode dumpmypdoscode.o

mypicodoscode.c: mypdos-atarisio.bin bootloader.bin dumpmypdoscode
	./dumpmypdoscode


dumphscode: dumphscode.o
	$(CC) -o dumphscode dumphscode.o

atarisio-highsio.bin: atarisio-highsio.src
	$(ATASM) -dRELOCTABLE -dSIOADR=4096 -oatarisio-highsio.bin atarisio-highsio.src

atarisio-highsio.c: atarisio-highsio.bin dumphscode
	./dumphscode


dumpspeedycode: dumpspeedycode.o
	$(CC) -o dumpspeedycode dumpspeedycode.o

speedy.bin: speedy.src speedy.inc eq1050.inc eqspeedy.inc
	$(ATASM) $(ASMFLAGS) -r speedy.src

speedycode.c: speedy.bin dumpspeedycode
	./dumpspeedycode

remote.com: remote.src
	$(ATASM) $(ASMFLAGS) -oremote.com remote.src

allclean: clean
	rm -f atarisio-highsio.bin bootloader.bin

clean:
	rm -f dumphscode *.o

