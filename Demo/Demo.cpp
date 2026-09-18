// Demo.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Windows.h>
#include <stdio.h>
#include "..\KDemo\KDemoCommon.h"

void DisplayBuffer(BYTE const* buf, DWORD size) {
	const int perLine = 16;
	for (DWORD i = 0; i < size; i++) {
		printf("%02X ", buf[i]);
		if (i % perLine == perLine - 1) {
			for (DWORD j = i - perLine + 1; j <= i; j++)
				printf("%c", isprint(buf[j]) ? (char)buf[j] : '.');
			printf("\n");
		}
	}
}

int main(int arc, const char* argv[]) {
	HANDLE hDevice = CreateFile(L"\\\\.\\KDemo", GENERIC_READ | GENERIC_WRITE,
		0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE) {
		printf("Error opening device (%u)\n", GetLastError());
		return 1;
	}

	BYTE data[256];
	ULONG_PTR address = strtoull(argv[1], nullptr, 0);
	DWORD ret;
	if (DeviceIoControl(hDevice, IOCTL_KDEMO_READ, &address, sizeof(address), data, sizeof(data),
		&ret, nullptr)) {
		DisplayBuffer(data, sizeof(data));
	}

	// do stuff
	CloseHandle(hDevice);
	return 0;
}
