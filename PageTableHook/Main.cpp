#include <ntifs.h>
#include <cstdint>
#include <intrin.h>
#include "PageHook.h"

NTSTATUS(*NtCreateFileOrig)(
	PHANDLE            FileHandle,
	ACCESS_MASK        DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PIO_STATUS_BLOCK   IoStatusBlock,
	PLARGE_INTEGER     AllocationSize,
	ULONG              FileAttributes,
	ULONG              ShareAccess,
	ULONG              CreateDisposition,
	ULONG              CreateOptions,
	PVOID              EaBuffer,
	ULONG              EaLength
	);

NTSTATUS NtCreateFileHook(
	PHANDLE            FileHandle,
	ACCESS_MASK        DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PIO_STATUS_BLOCK   IoStatusBlock,
	PLARGE_INTEGER     AllocationSize,
	ULONG              FileAttributes,
	ULONG              ShareAccess,
	ULONG              CreateDisposition,
	ULONG              CreateOptions,
	PVOID              EaBuffer,
	ULONG              EaLength
)
{
	static WCHAR BlockedFileName[] = L"test.txt";
	static SIZE_T BlockedFileNameLength = (sizeof(BlockedFileName) / sizeof(BlockedFileName[0])) - 1;

	PWCH NameBuffer;
	USHORT NameLength;

	__try
	{

		ProbeForRead(ObjectAttributes, sizeof(OBJECT_ATTRIBUTES), 1);
		ProbeForRead(ObjectAttributes->ObjectName, sizeof(UNICODE_STRING), 1);

		NameBuffer = ObjectAttributes->ObjectName->Buffer;
		NameLength = ObjectAttributes->ObjectName->Length;

		ProbeForRead(NameBuffer, NameLength, 1);
		NameLength /= sizeof(WCHAR);

		if (NameLength >= BlockedFileNameLength &&
			_wcsnicmp(&NameBuffer[NameLength - BlockedFileNameLength], BlockedFileName, BlockedFileNameLength) == 0)
		{
			DbgPrintEx(77,0,"Blocked access to %ws\n", BlockedFileName);
			return STATUS_ACCESS_DENIED;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		NOTHING;
	}

	return NtCreateFileOrig(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, AllocationSize, FileAttributes,
		ShareAccess, CreateDisposition, CreateOptions, EaBuffer, EaLength);
}




int drv_main()
{
	HANDLE process_id = reinterpret_cast<HANDLE>(404);
	PEPROCESS target_process = nullptr;
	if (PsLookupProcessByProcessId(process_id, &target_process) == STATUS_SUCCESS)
	{
		KAPC_STATE apc_state;
		KeStackAttachProcess(target_process,&apc_state);
		PageHook::add_page_hook(NtCreateFile, NtCreateFileHook, (void**)&NtCreateFileOrig);
		KeUnstackDetachProcess(&apc_state);
	}
	return 0;
}