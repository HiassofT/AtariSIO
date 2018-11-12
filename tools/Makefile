########################################################################
# AtariSIO tools Makefile
# (c) 2002-2013 by Matthias Reichl <hias@horus.com>
# http://www.horus.com/~hias/atari/
########################################################################

STRIP?=strip

ifdef ENABLE_ATP
ATPIMAGE_OBJS = AtpImage.o AtpTrack.o AtpSector.o \
	Atari1050Model.o \
	ChunkReader.o ChunkWriter.o Indent.o Crc32.o
ATPSERVER_OBJS = AtpSIOHandler.o AtpUtils.o
CXXFLAGS += -DENABLE_ATP
else
ATPIMAGE_OBJS =
ATPSERVER_OBJS =
endif

ifdef ALL_IN_ONE
EXECUTABLES = atarisio
CXXFLAGS += -DALL_IN_ONE
else
EXECUTABLES = atariserver atarixfer adir dir2atr ataricom

ifdef ENABLE_TESTS
EXECUTABLES += measure-system-latency casinfo test-fsk test-transmit \
	serialwatcher ataridd
endif

#MINGW_CXX=i586-mingw32msvc-g++
MINGW_CXX=i686-w64-mingw32-g++
#MINGW_STRIP=i586-mingw32msvc-strip
MINGW_STRIP=i686-w64-mingw32-strip

ifdef ENABLE_ATP
EXECUTABLES += atr2atp atpdump
endif

endif

SIOWRAPPER_OBJS = SIOWrapper.o KernelSIOWrapper.o UserspaceSIOWrapper.o

ifneq ($(DEFAULT_DEVICE),)
CXXFLAGS += -DDEFAULT_DEVICE=$(DEFAULT_DEVICE)
endif

CFLAGS += $(ZLIB_CFLAGS) $(NCURSES_CFLAGS)
CXXFLAGS += $(ZLIB_CFLAGS) $(NCURSES_CFLAGS)

COMMON_OBJS = DiskImage.o FileIO.o SIOTracer.o FileTracer.o Error.o

ATRIMAGE_OBJS = AtrImage.o AtrMemoryImage.o DCMCodec.o \
	CasBlock.o CasDataBlock.o CasFskBlock.o CasImage.o

ATARIXFER_OBJS = atarixfer.o \
	$(COMMON_OBJS) $(SIOWRAPPER_OBJS) $(ATRIMAGE_OBJS) \
	MiscUtils.o Directory.o \
	Dos2xUtils.o VirtualImageObserver.o MyPicoDosCode.o 

MEASURE_SYSTEM_LATENCY_OBJS = measure-system-latency.o \
	$(COMMON_OBJS) $(SIOWRAPPER_OBJS) \
	MiscUtils.o Directory.o

TEST_FSK_OBJS = test-fsk.o \
	$(COMMON_OBJS) $(SIOWRAPPER_OBJS) \
	MiscUtils.o Directory.o

TURBO_OBJS = turbo.o \
	$(COMMON_OBJS) $(SIOWRAPPER_OBJS) $(ATRIMAGE_OBJS)

SPEEDY_OBJS = speedy.o \
	$(COMMON_OBJS) $(SIOWRAPPER_OBJS) $(ATRIMAGE_OBJS) \
	DiskDriveCode.o

ATARIDD_OBJS = ataridd.o \
	$(COMMON_OBJS) $(SIOWRAPPER_OBJS)

SMAG24_OBJS = smag24.o \
	$(COMMON_OBJS) $(SIOWRAPPER_OBJS) $(ATRIMAGE_OBJS) $(ATPIMAGE_OBJS)

TEST_TRANSMIT_OBJS = test-transmit.o \
	 $(COMMON_OBJS) $(SIOWRAPPER_OBJS)

ATARISERVER_OBJS = atariserver.o CursesFrontend.o StringInput.o \
	History.o Directory.o DirectoryCache.o \
	FileInput.o FileSelect.o MiscUtils.o \
	$(COMMON_OBJS) $(SIOWRAPPER_OBJS) $(ATRIMAGE_OBJS) \
	$(ATPIMAGE_OBJS) $(ATPSERVER_OBJS) \
	DeviceManager.o SIOManager.o \
	AbstractSIOHandler.o AtrSIOHandler.o \
	PrinterHandler.o Coprocess.o RemoteControlHandler.o \
	DataContainer.o HighSpeedSIOCode.o MyPicoDosCode.o \
	CursesFrontendTracer.o AtrSearchPath.o SearchPath.o \
	Dos2xUtils.o VirtualImageObserver.o \
	CasHandler.o

COMMON_LIBS = $(ZLIB_LDFLAGS)

ATARISERVER_LIBS = $(COMMON_LIBS) $(NCURSES_LDFLAGS)

ATARISERVER_NOCURSES_OBJS = atariserver-nocurses.o \
	$(COMMON_OBJS) $(SIOWRAPPER_OBJS) $(ATRIMAGE_OBJS) \
	$(ATPIMAGE_OBJS) $(ATPSERVER_OBJS) \
	DeviceManager.o SIOManager.o \
	AbstractSIOHandler.o AtrSIOHandler.o \
	PrinterHandler.o Coprocess.o MiscUtils.o \
	HighSpeedSIOCode.o MyPicoDosCode.o \
	AtrSearchPath.o SearchPath.o Directory.o \
	Dos2xUtils.o VirtualImageObserver.o \
	CasHandler.o

ATARISERVER_NOCURSES_LIBS = $(COMMON_LIBS) -lreadline

ATR2ATP_OBJS = atr2atp.o AtpUtils.o \
	$(COMMON_OBJS) $(ATRIMAGE_OBJS) $(ATPIMAGE_OBJS) \
	Directory.o Dos2xUtils.o VirtualImageObserver.o MyPicoDosCode.o \
	MiscUtils.o

ATPDUMP_OBJS = atpdump.o $(COMMON_OBJS) $(ATPIMAGE_OBJS)

ADIR_OBJS = adir.o $(COMMON_OBJS) $(ATRIMAGE_OBJS) \
	Dos2xUtils.o VirtualImageObserver.o Directory.o MiscUtils.o \
	MyPicoDosCode.o

DIR2ATR_OBJS = dir2atr.o $(COMMON_OBJS) $(ATRIMAGE_OBJS) \
	Dos2xUtils.o VirtualImageObserver.o \
	Directory.o MiscUtils.o MyPicoDosCode.o

SERIALWATCHER_OBJS = serialwatcher.o SIOTracer.o FileTracer.o Error.o \
	MiscUtils.o Directory.o

CASINFO_OBJS = $(COMMON_OBJS) $(SIOWRAPPER_OBJS) \
	CasImage.o CasBlock.o CasDataBlock.o CasFskBlock.o \
	CasHandler.o MiscUtils.o casinfo.o

ATARICOM_OBJS = ComBlock.o Error.o AtariComMemory.o FileIO.o \
	ataricom.o

ALL_IN_ONE_OBJS = atarisio.o $(ATARISERVER_OBJS) atarixfer.o adir.o dir2atr.o \
	ComBlock.o AtariComMemory.o ataricom.o

ifdef ENABLE_ATP
ALL_IN_ONE_OBJS += atr2atp.o atpdump.o
endif

all: asmcode $(EXECUTABLES)

asmcode:
	$(MAKE) -C 6502

atarixfer: $(ATARIXFER_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(ATARIXFER_OBJS) $(COMMON_LIBS)

atariserver: $(ATARISERVER_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(ATARISERVER_OBJS) $(ATARISERVER_LIBS)

atariserver-nocurses: $(ATARISERVER_NOCURSES_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(ATARISERVER_NOCURSES_OBJS) $(ATARISERVER_NOCURSES_LIBS)

measure-system-latency: $(MEASURE_SYSTEM_LATENCY_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(MEASURE_SYSTEM_LATENCY_OBJS) $(COMMON_LIBS)

test-transmit: $(TEST_TRANSMIT_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(TEST_TRANSMIT_OBJS) $(COMMON_LIBS)

test-fsk: $(TEST_FSK_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(TEST_FSK_OBJS) $(COMMON_LIBS)

atr2atp: $(ATR2ATP_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(ATR2ATP_OBJS) $(COMMON_LIBS)

atpdump: $(ATPDUMP_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(ATPDUMP_OBJS) $(COMMON_LIBS)

adir: $(ADIR_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(ADIR_OBJS) $(COMMON_LIBS)

dir2atr: $(DIR2ATR_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(DIR2ATR_OBJS) $(COMMON_LIBS)

DIR2ATR_OBJS = dir2atr.o $(COMMON_OBJS) $(ATRIMAGE_OBJS) \
        Dos2xUtils.o VirtualImageObserver.o \
        Directory.o MiscUtils.o MyPicoDosCode.o

COMMON_OBJS = DiskImage.o FileIO.o SIOTracer.o FileTracer.o Error.o

ATRIMAGE_OBJS = AtrImage.o AtrMemoryImage.o DCMCodec.o \
        CasBlock.o CasDataBlock.o CasFskBlock.o CasImage.o

serialwatcher: $(SERIALWATCHER_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(SERIALWATCHER_OBJS)

casinfo: $(CASINFO_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(CASINFO_OBJS) $(COMMON_LIBS)

ataricom: $(ATARICOM_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(ATARICOM_OBJS) $(COMMON_LIBS)

turbo: $(TURBO_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(TURBO_OBJS) $(ATARISERVER_LIBS)

speedy: $(SPEEDY_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(SPEEDY_OBJS) $(ATARISERVER_LIBS)

ataridd: $(ATARIDD_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(ATARIDD_OBJS) $(ATARISERVER_LIBS)

smag24: $(SMAG24_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(SMAG24_OBJS) $(ATARISERVER_LIBS)

atarisio: $(ALL_IN_ONE_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(ALL_IN_ONE_OBJS) $(ATARISERVER_LIBS)

txtiming: txtiming.cpp
	$(CXX) -O2 -W -Wall -g -o $@ txtiming.cpp

cleanthis:
	rm -f *.o $(EXECUTABLES) *.exe

allclean: cleanthis
	$(MAKE) -C 6502 allclean

clean: cleanthis
	$(MAKE) -C 6502 clean

install: $(EXECUTABLES)
ifdef ALL_IN_ONE
	if test ! -d $(INST_DIR)/bin ; then \
		mkdir -p $(INST_DIR)/bin ; \
	fi
	install -o root -g users -m 4750 atarisio $(INST_DIR)/bin/atarisio
	ln -s -f $(INST_DIR)/bin/atarisio $(INST_DIR)/bin/atariserver
	ln -s -f $(INST_DIR)/bin/atarisio $(INST_DIR)/bin/atarixfer
	ln -s -f $(INST_DIR)/bin/atarisio $(INST_DIR)/bin/adir
	ln -s -f $(INST_DIR)/bin/atarisio $(INST_DIR)/bin/dir2atr
#ifdef ENABLE_ATP
#	ln -s -f $(INST_DIR)/bin/atarisio $(INST_DIR)/bin/atr2atp
#	ln -s -f $(INST_DIR)/bin/atarisio $(INST_DIR)/bin/atpdump
#endif
else
	if test ! -d $(INST_DIR)/bin ; then \
		mkdir -p $(INST_DIR)/bin ; \
	fi
	install -o root -g users -m 4750 atariserver $(INST_DIR)/bin/atariserver
	install -o root -g users -m 4750 atarixfer $(INST_DIR)/bin/atarixfer
	install -o root -g users -m 755 adir $(INST_DIR)/bin/adir
	install -o root -g users -m 755 dir2atr $(INST_DIR)/bin/dir2atr
	install -o root -g users -m 755 ataricom $(INST_DIR)/bin/ataricom
#ifdef ENABLE_ATP
#	install -o root -g users -m 755 atr2atp $(INST_DIR)/bin/atr2atp
#	install -o root -g users -m 755 atpdump $(INST_DIR)/bin/atpdump
#endif
endif
	if test ! -d $(INST_DIR)/include ; then \
		mkdir -p $(INST_DIR)/include ; \
	fi

uninstall:
	rm -f $(INST_DIR)/bin/atariserver 
	rm -f $(INST_DIR)/bin/atarixfer
#	rm -f $(INST_DIR)/bin/atr2atp
#	rm -f $(INST_DIR)/bin/atpdump
	rm -f $(INST_DIR)/bin/adir
	rm -f $(INST_DIR)/bin/dir2atr
	rm -f $(INST_DIR)/bin/ataricom

dep:
	rm -f .depend
	for i in *.cpp ; do \
		$(CXX) -I$(KDIR)/includes -MM $$i >> .depend ; \
	done 

-include .depend

