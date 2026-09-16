#include <ntifs.h>
#include "KSimpleComnon.h"

#define KSIMPLE_PREFIX "KSimple: "
#define KSIMPLE_NAME L"KSimple"

NTSTATUS AddProtectedProcess(ULONG pid);
NTSTATUS UnprotectAll();
NTSTATUS RemoveProtectedProcess(ULONG pid);
OB_PREOP_CALLBACK_STATUS OnPreOpenProcess(_In_ PVOID ctx, _Inout_ POB_PRE_OPERATION_INFORMATION info);
bool IsProtectedProcess(ULONG pid);

const int MaxProtectedProcesses = 16;
ULONG ProtectedPids[MaxProtectedProcesses];
int TotalProtectedProcesses;
PVOID ObRegCookie;

void OnUnload(PDRIVER_OBJECT drvObj) {
	ObUnRegisterCallbacks(ObRegCookie);
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
	auto len = (ULONG_PTR)0;

	bool read = false;
	switch (dic.IoControlCode) {
		case IOCTL_KSIMPLE_PROTECT_PID: 
		case IOCTL_KSIMPLE_UNPROTECT_PID:
		{
			if (dic.InputBufferLength < sizeof(ULONG)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			auto pid = *(ULONG*)irp->AssociatedIrp.SystemBuffer;
			status = dic.IoControlCode == IOCTL_KSIMPLE_PROTECT_PID ? AddProtectedProcess(pid) : RemoveProtectedProcess(pid);
			break;
		}

		case IOCTL_KSIMPLE_UNPROTECT_ALL:
			status = UnprotectAll();
			break;

		case IOCTL_KSIMPLE_READ:
			read = true;
		case IOCTL_KSIMPLE_WRITE:
			if (dic.InputBufferLength < sizeof(KSimpleReadWrite)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			auto rw = (KSimpleReadWrite*)irp->AssociatedIrp.SystemBuffer;
			if (dic.OutputBufferLength < rw->Size) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			auto output = MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority);
			if (output == nullptr) {
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}
			status = MmCopyMemory(read ? output : rw->Address, MM_COPY_ADDRESS{ read ? rw->Address : output }, rw->Size, MM_COPY_MEMORY_VIRTUAL, &len);
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

	OB_OPERATION_REGISTRATION op{
		.ObjectType = PsProcessType,
		.Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE,
		.PreOperation = OnPreOpenProcess,
		.PostOperation = nullptr
	};

	OB_CALLBACK_REGISTRATION reg{
		.Version = OB_FLT_REGISTRATION_VERSION,
		.OperationRegistrationCount = 1,
		.Altitude = RTL_CONSTANT_STRING(L"12345.678"),
		.OperationRegistration = &op,
	};
	status = ObRegisterCallbacks(&reg, &ObRegCookie);
	if (!NT_SUCCESS(status)) {
		DbgPrint(KSIMPLE_PREFIX "Failed in ObRegisterCallbacks (0x%X)\n", status);
		IoDeleteDevice(devObj);
		IoDeleteSymbolicLink(&symName);
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

NTSTATUS UnprotectAll() {
	TotalProtectedProcesses = 0;
	memset(ProtectedPids, 0, sizeof(ProtectedPids));
	return STATUS_SUCCESS;
}

NTSTATUS RemoveProtectedProcess(ULONG pid) {
	if (TotalProtectedProcesses) {
		for (int i = 0; i < MaxProtectedProcesses; i++) {
			if (ProtectedPids[i] == pid) {
				ProtectedPids[i] = 0;
				TotalProtectedProcesses--;
				return STATUS_SUCCESS;
			}
		}
	}
	return STATUS_INVALID_PARAMETER;
}

_Use_decl_annotations_
OB_PREOP_CALLBACK_STATUS OnPreOpenProcess(PVOID, POB_PRE_OPERATION_INFORMATION info) {
	if (info->KernelHandle)
		return OB_PREOP_SUCCESS;

	const ULONG PROCESS_TERMINATE = 1;

	auto process = (PEPROCESS)info->Object;
	if (IsProtectedProcess(HandleToULong(PsGetProcessId(process)))) {
		info->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_TERMINATE;
	}

	return OB_PREOP_SUCCESS;
}

bool IsProtectedProcess(ULONG pid) {
	if (TotalProtectedProcesses) {
		for (int i = 0; i < MaxProtectedProcesses; i++) {
			if (ProtectedPids[i] == pid) {
				return true;
			}
		}
	}
	return false;
}
