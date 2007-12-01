all: libatarisio.dll aconv.exe

CXX=i586-mingw32msvc-g++
CC=i586-mingw32msvc-gcc

CFLAGS=-W -Wall -DBUILDING_DLL -DWINVER
CXXFLAGS=-W -Wall -DBUILDING_DLL -DWINVER

DLLOBJS= Error.o FileIO.o SIOTracer.o StringTracer.o DataContainer.o \
	 DiskImage.o AtrImage.o AtrMemoryImage.o DCMCodec.o \
	 Dos2xUtils.o VirtualImageObserver.o \
	 Directory.o MiscUtils.o MyPicoDosCode.o \
	 libatarisio.o

libatarisio.dll: $(DLLOBJS)
	$(CXX) -shared -o libatarisio.dll \
	-Wl,--out-implib,libatarisio.a,--output-def,libatarisiox.def \
	$(DLLOBJS)
	sed -e 's/@.*$$//' libatarisiox.def > libatarisio.def

#	-Wl,--out-implib,libatarisio.a,--output-def,libatarisio.def,--add-stdcall-alias \
#	-Wl,--out-implib,libatarisio.a,--output-def,libatarisio.def \
#	-Wl,--out-implib,libatarisio.a \

aconv.o: aconv.c libatarisio.h
	$(CC) -W -Wall -DWINVER -c aconv.c

aconv.exe: aconv.o libatarisio.a
	$(CC) -o aconv.exe aconv.o -L. libatarisio.a

clean:
	rm -f *.o *.dll *.a *.def

dep:
	rm -f .depend
	for i in *.cpp ; do \
		$(CXX) -DWINVER -DBUILDING_DLL -MM $$i >> .depend ; \
	done

zip:
	zip dll.zip libatarisio.dll libatarisio.lib libatarisio.def libatarisio.exp \
	libatarisio.h aconv.c aconv.exe

-include .depend

