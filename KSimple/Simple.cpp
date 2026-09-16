#include <ntifs.h>
#include "KSimpleComnon.h"

#define KSIMPLE_PREFIX "KSimple: "
#define KSIMPLE_NAME L"KSimple"

NTSTATUS AddProtectedProcess(ULONG pid);

const int MaxProtectedProcesses = 16;
ULONG ProtectedPids[MaxProtectedProcesses];
int TotalProtectedProcesses;

void OnUnload(PDRIVER_OBJECT drvObj) {
	UNICODE_STRING symName = RTL_CONSTANT_STRING(L"\\??\\" KSIMPLE_NAME);
	IoDeleteSymbolicLink(&symName);
	IoDeleteDevice(drvObj->DeviceObject);
}

NTSTATUS OnCreateClose(PDEVICE_OBJECT, PIRP irp) {
	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS OnDeviceControl(PDEVICE_OBJECT, PIRP irp) {
	auto status = STATUS_INVALID_DEVICE_REQUEST;
	auto& dic = IoGetCurrentIrpStackLocation(irp)->Parameters.DeviceIoControl;
	auto len = 0U;

	switch (dic.IoControlCode) {
	case IOCTL_KSIMPLE_PROTECT_PID:
		auto pid = *(ULONG*)irp->AssociatedIrp.SystemBuffer;
		status = AddProtectedProcess(pid);
		break;
	}

	irp->IoStatus.Status = status;
	irp->IoStatus.Information = len;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return status;
}

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT drvObj, PUNICODE_STRING regPath) {
	DbgPrint(KSIMPLE_PREFIX "DriverEntry: drvObj: 0x%p regPath: %wZ\n", drvObj, regPath);

	drvObj->DriverUnload = OnUnload;
	drvObj->MajorFunction[IRP_MJ_CREATE] = drvObj->MajorFunction[IRP_MJ_CLOSE] = OnCreateClose;
	drvObj->MajorFunction[IRP_MJ_DEVICE_CONTROL] = OnDeviceControl;

	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\" KSIMPLE_NAME);
	PDEVICE_OBJECT devObj;
	auto status = IoCreateDevice(drvObj, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &devObj);
	if (!NT_SUCCESS(status)) {
		DbgPrint(KSIMPLE_PREFIX "Failed to create device (0x%X)\n", status);
		return status;
	}

	UNICODE_STRING symName = RTL_CONSTANT_STRING(L"\\??\\" KSIMPLE_NAME);
	status = IoCreateSymbolicLink(&symName, &devName);
	if (!NT_SUCCESS(status)) {
		DbgPrint(KSIMPLE_PREFIX "Failed to create symlink (0x%X)\n", status);
		IoDeleteDevice(devObj);
		return status;
	}

	return STATUS_SUCCESS;
}

NTSTATUS AddProtectedProcess(ULONG pid) {
	if (TotalProtectedProcesses == MaxProtectedProcesses)
		return STATUS_NO_MEMORY;

	PEPROCESS process;
	auto status = PsLookupProcessByProcessId(ULongToHandle(pid), &process);
	if (!NT_SUCCESS(status))
		return status;

	for (int i = 0; i < MaxProtectedProcesses; i++) {
		if (ProtectedPids[i] == 0) {
			ProtectedPids[i] = pid;
			TotalProtectedProcesses++;
			return STATUS_SUCCESS;
		}
	}
	return STATUS_UNSUCCESSFUL;
}
