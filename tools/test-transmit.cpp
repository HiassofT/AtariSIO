#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "SIOWrapper.h"

#define BUFLEN 257
unsigned char buf[BUFLEN];

int main(int argc, char** argv)
{
	if (argc < 3) {
		printf("usage: test-send device baudrate\n");
		return 1;
	}
	const char* device = argv[1];
	unsigned int baudrate = atoi(argv[2]);

	RCPtr<SIOWrapper> sio = SIOWrapper::CreateSIOWrapper(device);

	sio->SetSIOServerMode(SIOWrapper::eCommandLine_None);
	sio->SetBaudrate(baudrate);

	memset(buf, 0, BUFLEN);
	//buf[BUFLEN-1] = 0x8f;

	while (1) {
		sio->SendRawDataNoWait(buf, BUFLEN);
		//sio->FlushWriteBuffer();
		//usleep(1000);
	}

	return 0;
}
