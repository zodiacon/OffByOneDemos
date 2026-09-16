// CopyToken.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <phnt_windows.h>
#include <phnt.h>
#include <stdio.h>
#include "..\KSimple\KSimpleComnon.h"

#pragma comment(lib, "ntdll")

PVOID GetProcessAddress(HANDLE hProcess) {
	ULONG size = 1 << 24;
	auto buf = (SYSTEM_HANDLE_INFORMATION_EX*)VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!buf)
		return nullptr;
	if (STATUS_SUCCESS != NtQuerySystemInformation(SystemExtendedHandleInformation, buf, size, &size))
		return nullptr;

	for (ULONG_PTR i = 0; i < buf->NumberOfHandles; i++) {
		auto& handle = buf->Handles[i];
		if (HandleToUlong(handle.UniqueProcessId) == GetCurrentProcessId() && handle.HandleValue == hProcess) {
			return handle.Object;
		}
	}
	return nullptr;
}

int main(int argc, const char* argv[]) {
	if (argc < 3) {
		printf("Usage: copytoken <src_pid> <dst_pid>\n");
		return 0;
	}

	auto hDevice = CreateFile(L"\\\\.\\KSimple", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE) {
		printf("Error opening device (%u)\n", GetLastError());
		return 1;
	}

	auto src = strtoul(argv[1], nullptr, 0);
	auto hSrc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, src);
	if (!hSrc && GetLastError() != ERROR_ACCESS_DENIED) {
		printf("Error locating source (%u)\n", GetLastError());
		return 1;
	}
	auto srcAddress = GetProcessAddress(hSrc);
	if (!srcAddress) {
		printf("Unable to get src process address\n");
		return 1;
	}

	DWORD ret;

	auto dst = strtoul(argv[2], nullptr, 0);
	auto hDst = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dst);
	if (!hDst && GetLastError() != ERROR_ACCESS_DENIED) {
		printf("Error locating destination (%u)\n", GetLastError());
		return 1;
	}
	auto dstAddress = GetProcessAddress(hDst);
	if (!dstAddress) {
		printf("Unable to get dst process address\n");
		return 1;
	}

	ULONG tokenOffset = 0x248;
	ULONG_PTR orgToken;
	KSimpleReadWrite rworg{
		.Address = (PBYTE)dstAddress + tokenOffset,
		.Size = sizeof(orgToken)
	};
	DeviceIoControl(hDevice, IOCTL_KSIMPLE_READ, &rworg, sizeof(rworg), &orgToken, sizeof(orgToken), &ret, nullptr);

	ULONG_PTR token;
	KSimpleReadWrite rw{
		.Address = (PBYTE)srcAddress + tokenOffset,
		.Size = sizeof(token)
	};
	BOOL success = FALSE;
	success = DeviceIoControl(hDevice, IOCTL_KSIMPLE_READ, &rw, sizeof(rw), &token, sizeof(token), &ret, nullptr);
	if (success) {
		rw.Address = (PBYTE)dstAddress + tokenOffset;
		token &= ~0xf;
		token |= (orgToken & 0xf) + 1;
		success = DeviceIoControl(hDevice, IOCTL_KSIMPLE_WRITE, &rw, sizeof(rw), &token, sizeof(token), &ret, nullptr);
	}
	if (success) {
		printf("Success.\n");
	}
	else {
		printf("Error (%u)\n", GetLastError());
	}
	CloseHandle(hDevice);
	return 0;

}
