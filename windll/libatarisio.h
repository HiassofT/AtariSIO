#include <windows.h>

#ifdef BUILDING_DLL
#define DLLEXPORT __declspec(dllexport) WINAPI __stdcall
#else
#define DLLEXPORT __declspec(dllimport) WINAPI __stdcall
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef BUILDING_DLL
DLLEXPORT BOOL DllMain(HINSTANCE, DWORD, LPVOID);
#endif

DLLEXPORT int AtariSIO_CreateImage(int bytesPerSector, int sectors);
DLLEXPORT int AtariSIO_LoadImage(const char* filename);
DLLEXPORT int AtariSIO_SaveImage(int imageHandle, const char* filename);
DLLEXPORT int AtariSIO_FreeImage(int imageHandle);

#define AtariSIOError_OK 0
#define AtariSIOError_TooManyOpenHandles -1
#define AtariSIOError_InvalidHandle -2
#define AtariSIOError_InvalidImageSize -3
#define AtariSIOError_InvalidImageSize -3
#define AtariSIOError_UnknownError -99

DLLEXPORT int AtariSIO_GetLastError();

// 0 means no warnings
// 1 means warnings available
// -1 means buffer too small
// -2 means invalid parameters (buf = NUL or maxlen < 2)
DLLEXPORT int AtariSIO_GetWarnings(char* buf, int maxlen);

#ifdef __cplusplus
}
#endif
