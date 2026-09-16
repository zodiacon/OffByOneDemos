// ReadWrite.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <stdio.h>
#include <Windows.h>
#include <memory>
#include "..\KSimple\KSimpleComnon.h"

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

int main(int argc, const char* argv[]) {
	if (argc < 4) {
		printf("Usage: readwrite r|w <address> <size>\n");
		return 0;
	}

	auto hDevice = CreateFile(L"\\\\.\\KSimple", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE) {
		printf("Error opening device (%u)\n", GetLastError());
		return 1;
	}

	bool write = argv[1][0] == 'w' || argv[1][0] == 'W';
	KSimpleReadWrite rw;
	rw.Address = (PVOID)strtoull(argv[2], nullptr, 0);
	rw.Size = strtoul(argv[3], nullptr, 0);

	DWORD ret;
	auto buffer = std::make_unique<BYTE[]>(rw.Size);
	auto success = DeviceIoControl(hDevice, write ? IOCTL_KSIMPLE_WRITE : IOCTL_KSIMPLE_READ,
		&rw, sizeof(rw), buffer.get(), rw.Size, &ret, nullptr);

	if (success) {
		printf("Success.\n");
		if (!write)
			DisplayBuffer(buffer.get(), ret);
	}
	CloseHandle(hDevice);
	return 0;
}

