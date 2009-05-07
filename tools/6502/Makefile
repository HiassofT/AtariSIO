ATASM=atasm
#ATASM=/data/src/xl/atasm103/src/atasm

CFLAGS=-W -Wall

all: atarisio-highsio.c mypicodoscode.c remote.com
# speedycode.c

#ASMFLAGS=
ASMFLAGS = -s

bootloader.bin: bootloader.src
	$(ATASM) -r -f0 -obootloader.bin bootloader.src

dumpmypdoscode: dumpmypdoscode.o
	$(CC) -o dumpmypdoscode dumpmypdoscode.o

mypicodoscode.c: atarisio-mypdos.bin bootloader.bin dumpmypdoscode
	./dumpmypdoscode


dumphscode: dumphscode.o
	$(CC) -o dumphscode dumphscode.o

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
	rm -f bootloader.bin

clean:
	rm -f dumphscode *.o

