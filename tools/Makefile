########################################################################
# AtariSIO tools Makefile
# (c) 2002-2004 by Matthias Reichl <hias@horus.com>
# http://www.horus.com/~hias/atari/
########################################################################

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
EXECUTABLES = atariserver atarixfer adir dir2atr measure-system-latency casinfo ataricom
#	atariserver-nocurses
#	ataridd smag24 
#	speedy
#	turbo
#	serialwatcher

ifdef ENABLE_ATP
EXECUTABLES += atr2atp atpdump
endif

endif

COMMON_OBJS = DiskImage.o FileIO.o SIOTracer.o FileTracer.o Error.o

ATRIMAGE_OBJS = AtrImage.o AtrMemoryImage.o DCMCodec.o \
	CasBlock.o CasImage.o

ATARIXFER_OBJS = atarixfer.o $(COMMON_OBJS) $(ATRIMAGE_OBJS) \
	SIOWrapper.o MiscUtils.o Directory.o \
	Dos2xUtils.o VirtualImageObserver.o MyPicoDosCode.o 

MEASURE_SYSTEM_LATENCY_OBJS = measure-system-latency.o $(COMMON_OBJS) \
	SIOWrapper.o MiscUtils.o Directory.o

TURBO_OBJS = turbo.o $(COMMON_OBJS) $(ATRIMAGE_OBJS) \
	SIOWrapper.o

SPEEDY_OBJS = speedy.o $(COMMON_OBJS) $(ATRIMAGE_OBJS) \
	SIOWrapper.o DiskDriveCode.o

ATARIDD_OBJS = ataridd.o $(COMMON_OBJS) SIOWrapper.o

SMAG24_OBJS = smag24.o \
	$(COMMON_OBJS) $(ATRIMAGE_OBJS) $(ATPIMAGE_OBJS) \
	SIOWrapper.o

ATARISERVER_OBJS = atariserver.o CursesFrontend.o StringInput.o \
	History.o Directory.o DirectoryCache.o \
	FileInput.o FileSelect.o MiscUtils.o \
	$(COMMON_OBJS) $(ATRIMAGE_OBJS) \
	$(ATPIMAGE_OBJS) $(ATPSERVER_OBJS) \
	DeviceManager.o SIOWrapper.o SIOManager.o \
	AbstractSIOHandler.o AtrSIOHandler.o \
	PrinterHandler.o Coprocess.o RemoteControlHandler.o \
	DataContainer.o HighSpeedSIOCode.o MyPicoDosCode.o \
	CursesFrontendTracer.o AtrSearchPath.o SearchPath.o \
	Dos2xUtils.o VirtualImageObserver.o \
	CasHandler.o

ATARISERVER_LIBS = $(COMMON_LIBS) -lpanel -lncurses

ATARISERVER_NOCURSES_OBJS = atariserver-nocurses.o \
	$(COMMON_OBJS) $(ATRIMAGE_OBJS) \
	$(ATPIMAGE_OBJS) $(ATPSERVER_OBJS) \
	DeviceManager.o SIOWrapper.o SIOManager.o \
	AbstractSIOHandler.o AtrSIOHandler.o \
	PrinterHandler.o Coprocess.o MiscUtils.o \
	HighSpeedSIOCode.o MyPicoDosCode.o \
	AtrSearchPath.o SearchPath.o Directory.o \
	Dos2xUtils.o VirtualImageObserver.o

ATARISERVER_NOCURSES_LIBS = $(COMMON_LIBS) -lreadline -lncurses

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

CASINFO_OBJS = $(COMMON_OBJS) CasImage.o CasBlock.o CasHandler.o SIOWrapper.o \
	casinfo.o

ATARICOM_OBJS = ComBlock.o Error.o AtariComMemory.o FileIO.o \
	ataricom.o

ALL_IN_ONE_OBJS = atarisio.o $(ATARISERVER_OBJS) atarixfer.o adir.o dir2atr.o

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

atr2atp: $(ATR2ATP_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(ATR2ATP_OBJS) $(COMMON_LIBS)

atpdump: $(ATPDUMP_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(ATPDUMP_OBJS) $(COMMON_LIBS)

adir: $(ADIR_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(ADIR_OBJS) $(COMMON_LIBS)

dir2atr: $(DIR2ATR_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(DIR2ATR_OBJS) $(COMMON_LIBS)

serialwatcher: $(SERIALWATCHER_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(SERIALWATCHER_OBJS)

casinfo: $(CASINFO_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(CASINFO_OBJS) $(COMMON_LIBS)

ataricom: $(ATARICOM_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(ATARICOM_OBJS) $(COMMON_LIBS)

ataricom.exe: ataricom.cpp Error.cpp AtariComMemory.cpp ComBlock.cpp FileIO.cpp
	i586-mingw32msvc-g++ -W -Wall -DATARISIO_DEBUG -o $@ $^

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
	rm -f *.o $(EXECUTABLES)

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

dep:
	rm -f .depend
	for i in *.cpp ; do \
		$(CXX) -I$(KDIR)/includes -MM $$i >> .depend ; \
	done 

-include .depend

