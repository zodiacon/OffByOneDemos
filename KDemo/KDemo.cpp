#include <ntddk.h>
#include "KDemoCommon.h"

void OnUnload(PDRIVER_OBJECT drvObj) {
	DbgPrint("KDemo: Unload\n");
	UNICODE_STRING symName = RTL_CONSTANT_STRING(L"\\??\\KDemo");
	IoDeleteSymbolicLink(&symName);
	IoDeleteDevice(drvObj->DeviceObject);
}

NTSTATUS OnCreateClose(PDEVICE_OBJECT, PIRP irp) {
	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, 0);
	return STATUS_SUCCESS;
}

NTSTATUS OnDeviceControl(PDEVICE_OBJECT, PIRP irp) {
	IO_STACK_LOCATION* irpSp = IoGetCurrentIrpStackLocation(irp);
	auto& dic = irpSp->Parameters.DeviceIoControl;
	auto status = STATUS_INVALID_DEVICE_REQUEST;
	auto len = 0ULL;
	switch (dic.IoControlCode) {
		case IOCTL_KDEMO_READ:
			if (dic.InputBufferLength < sizeof(PVOID)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			auto address = *(PVOID*)irp->AssociatedIrp.SystemBuffer;
			memcpy(irp->AssociatedIrp.SystemBuffer, address, len = dic.OutputBufferLength);
			status = STATUS_SUCCESS;

	}

	irp->IoStatus.Status = status;
	irp->IoStatus.Information = len;
	IoCompleteRequest(irp, 0);
	return status;
}

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT drvObj, PUNICODE_STRING regPath) {
	DbgPrint("KDemo: Driver=0x%p regPath=%wZ\n", drvObj, regPath);

	drvObj->DriverUnload = OnUnload;
	drvObj->MajorFunction[IRP_MJ_CREATE] = drvObj->MajorFunction[IRP_MJ_CLOSE] = OnCreateClose;
	drvObj->MajorFunction[IRP_MJ_DEVICE_CONTROL] = OnDeviceControl;

	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\KDemo");
	PDEVICE_OBJECT devObj;
	auto status = IoCreateDevice(drvObj, 0, &devName, FILE_DEVICE_UNKNOWN,
		0, FALSE, &devObj);
	if (!NT_SUCCESS(status)) {
		DbgPrint("Error in IoCreateDevice 0x%u\n", status);
		return status;
	}

	UNICODE_STRING symName = RTL_CONSTANT_STRING(L"\\??\\KDemo");
	status = IoCreateSymbolicLink(&symName, &devName);
	if (!NT_SUCCESS(status)) {
		DbgPrint("Error in IoCreateSymbolicLink 0x%u\n", status);
		IoDeleteDevice(devObj);
		return status;
	}

	return STATUS_SUCCESS;
}

