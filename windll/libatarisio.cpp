#include <windows.h>
#include "libatarisio.h"

#include "AtrMemoryImage.h"
#include "SIOTracer.h"
#include "StringTracer.h"

#define MAX_HANDLES 1024

static int lastError;

static SIOTracer* sioTracer;
static RCPtr<StringTracer> stringTracer;

static RCPtr<AtrMemoryImage> imageHandles[MAX_HANDLES];

static void clearError()
{
	lastError = AtariSIOError_OK;
	stringTracer->ClearString();
}

static void init_dll()
{
	int i;

	// clear all handles
	for (i=0;i<MAX_HANDLES;i++) {
		imageHandles[i].SetToNull();
	}

	// enable string tracer
	sioTracer = SIOTracer::GetInstance();
	stringTracer = new StringTracer;
	sioTracer->AddTracer(stringTracer);
	sioTracer->SetTraceGroup(SIOTracer::eTraceInfo, true, stringTracer);
	sioTracer->SetTraceGroup(SIOTracer::eTraceDebug, true, stringTracer);

	clearError();
}

BOOL WINAPI DllMain(
	HINSTANCE /*hinstDLL*/,
	DWORD fdwReason,
	LPVOID /*lpReserved*/ )
{
	switch( fdwReason ) {
	case DLL_PROCESS_ATTACH:
		init_dll();
		break;
	default: break;
	}
	return TRUE;
}


// internal handle management functions

static int checkImageHandle(int i)
{
	if (i >= 0 && i < MAX_HANDLES && imageHandles[i]) {
		return 0;
	} else {
		lastError = AtariSIOError_InvalidHandle;
		return 1;
	}
}

static int getImageHandle()
{
	int i=0;
	while (imageHandles[i] && i < MAX_HANDLES) i++;
	if (i < MAX_HANDLES) {
		return i;
	} else {
		lastError = AtariSIOError_TooManyOpenHandles;
		return -1;
	}
}

static void freeImageHandle(int i)
{
	if (checkImageHandle(i) == 0) {
		imageHandles[i].SetToNull();
	}
}


// public functions

DLLEXPORT int AtariSIO_GetLastError()
{
	return lastError;
}

DLLEXPORT int AtariSIO_CreateImage(int bytesPerSector, int sectors)
{
	ESectorLength seclen;

	clearError();

	switch (bytesPerSector) {
	case 128:
		seclen = e128BytesPerSector;
		break;
	case 256:
		seclen = e256BytesPerSector;
		break;
	default:
		lastError = AtariSIOError_InvalidImageSize;
		return -1;
	}
	if (sectors < 1 || sectors > 65535) {
		lastError = AtariSIOError_InvalidImageSize;
		return -1;
	}
	int handle = getImageHandle();
	if (handle < 0) {
		return -1;
	}

	RCPtr<AtrMemoryImage> img = new AtrMemoryImage();
	imageHandles[handle] = img;
	if (! img->CreateImage(seclen, sectors)) {
		lastError = AtariSIOError_UnknownError;
		freeImageHandle(handle);
		return -1;
	}
	return handle;
}

DLLEXPORT int AtariSIO_LoadImage(const char* filename)
{
	clearError();

	int handle = getImageHandle();
	if (handle < 0) {
		return -1;
	}

	RCPtr<AtrMemoryImage> img = new AtrMemoryImage();
	imageHandles[handle] = img;

	if (! img->ReadImageFromFile(filename)) {
		lastError = AtariSIOError_UnknownError;
		freeImageHandle(handle);
		return -1;
	}
	return handle;
}

DLLEXPORT int AtariSIO_SaveImage(int handle, const char* filename)
{
	clearError();
	if (checkImageHandle(handle)) {
		return -1;
	}

	RCPtr<AtrMemoryImage> img = imageHandles[handle];

	if (! img->WriteImageToFile(filename)) {
		lastError = AtariSIOError_UnknownError;
		freeImageHandle(handle);
		return -1;
	}
	return 0;
}


DLLEXPORT int AtariSIO_FreeImage(int handle)
{
	clearError();
	if (checkImageHandle(handle)) {
		return -1;
	}

	freeImageHandle(handle);

	return 0;
}

DLLEXPORT int AtariSIO_GetWarnings(char* buf, int maxlen)
{
	if (!buf) {
		return -1;
	}
	if (maxlen < 2) {
		return -1;
	}
	int len = stringTracer->GetStringLength();
	if (!len) {
		*buf = 0;
		return 0;
	}
	int l2 = len;
	int ret = 1;
	if (len+1 > maxlen) {
		l2 = maxlen - 1;
		ret = -1;
	}
	memcpy(buf, stringTracer->GetString(), l2);
	buf[l2] = 0;
	return ret;
}



