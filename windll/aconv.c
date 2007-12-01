#include <stdio.h>
#include "libatarisio.h"

#define BUFSIZE 1024
static char buf[BUFSIZE];

void CheckWarnings()
{
	int ret=AtariSIO_GetWarnings(buf, BUFSIZE);
	if (ret == -2) {
		printf("parameter error in AtariSIO_GetWarnings!");
		return;
	}
	if (ret != 0) {
		printf("messages from libatarisio:");
		if (ret == -1) {
			printf(" buffer too small");
		}
		printf("\n%s", buf);
	}
}

int main(int argc, char** argv)
{
	int handle, ret;
	if (argc != 3) {
		printf("usage: aconv in-file out-file\n");
		return 1;
	}
	handle = AtariSIO_LoadImage(argv[1]);
	CheckWarnings();
	if (handle < 0) {
		printf("error loading image (%d)\n", AtariSIO_GetLastError());
		return 1;
	}
	ret = AtariSIO_SaveImage(handle,argv[2]);
	CheckWarnings();
       	if (ret < 0) {
		printf("error writing image (%d)\n", AtariSIO_GetLastError());
		return 1;
	}
	ret = AtariSIO_FreeImage(handle);
	CheckWarnings();
       	if (ret < 0) {
		printf("error freeing image (%d)\n", AtariSIO_GetLastError());
		return 1;
	}
	printf("successfully converted image\n");
	return 0;
}
