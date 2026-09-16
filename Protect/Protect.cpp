// Protect.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <stdio.h>
#include <Windows.h>
#include "..\KSimple\KSimpleComnon.h"

int main(int argc, const char* argv[]) {
	if (argc < 2) {
		printf("Usage: protect p[rotect] <pid> | u[nprotect] <pid> | c[lear]\n");
		return 0;
	}

	auto hDevice = CreateFile(L"\\\\.\\KSimple", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE) {
		printf("Error opening device (%u)\n", GetLastError());
		return 1;
	}

	BOOL success = FALSE;
	DWORD ret;
	bool protect = false;
	switch (argv[1][0]) {
		case 'c': case 'C':
			success = DeviceIoControl(hDevice, IOCTL_KSIMPLE_UNPROTECT_ALL, nullptr, 0, nullptr, 0, &ret, nullptr);
			break;

		case 'p': case 'P':
			protect = true;
		case 'u': case 'U':
			if (argc < 3) {
				printf("Missing PID\n");
				return 1;
			}
			auto pid = strtoul(argv[2], nullptr, 0);
			success = DeviceIoControl(hDevice, protect ? IOCTL_KSIMPLE_PROTECT_PID : IOCTL_KSIMPLE_UNPROTECT_PID, 
				&pid, sizeof(pid), nullptr, 0, &ret, nullptr);
			break;
	}
	if (success)
		printf("Success.\n");
	else
		printf("Error (%u)\n", GetLastError());

	CloseHandle(hDevice);
	return 0;
}
