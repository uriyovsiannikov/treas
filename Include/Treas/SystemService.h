#ifndef _TREAS_SYSTEM_SERVICE_H_
#define _TREAS_SYSTEM_SERVICE_H_

#include <Treas/Types.h>

#define KI_SYSTEM_CALL_INVALID_RESULT 0xFFFFFFFFFFFFFFFEULL

typedef ULONGLONG (*PKI_SYSTEM_SERVICE_ROUTINE)(ULONG_PTR Argument1,
                                                ULONGLONG Argument2);

ULONGLONG KipWriteBufferService(ULONG_PTR Argument1, ULONGLONG Argument2);
ULONGLONG KipExitProcessService(ULONG_PTR Argument1, ULONGLONG Argument2);
ULONGLONG KipYieldThreadService(ULONG_PTR Argument1, ULONGLONG Argument2);
ULONGLONG KipCreateThreadService(ULONG_PTR Argument1, ULONGLONG Argument2);
ULONGLONG KipExitThreadService(ULONG_PTR Argument1, ULONGLONG Argument2);
ULONGLONG KipQuerySystemInformationService(ULONG_PTR Argument1,
                                           ULONGLONG Argument2);
ULONGLONG KipReadBufferService(ULONG_PTR Argument1, ULONGLONG Argument2);
ULONGLONG KipWriteErrorBufferService(ULONG_PTR Argument1, ULONGLONG Argument2);
ULONGLONG KipManageVirtualMemoryService(ULONG_PTR Argument1,
                                        ULONGLONG Argument2);
ULONGLONG KipHostFileService(ULONG_PTR Argument1, ULONGLONG Argument2);
ULONGLONG KipManageObjectService(ULONG_PTR Argument1, ULONGLONG Argument2);

#endif
