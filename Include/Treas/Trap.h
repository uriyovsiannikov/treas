#ifndef _TREAS_TRAP_H_
#define _TREAS_TRAP_H_

#include <Treas/Types.h>

typedef struct _KI_EXCEPTION_FRAME {
    ULONGLONG Vector;
    ULONGLONG ErrorCode;
    ULONGLONG InstructionPointer;
    ULONGLONG CodeSegment;
    ULONGLONG Flags;
    ULONGLONG StackPointer;
    ULONGLONG StackSegment;
} KI_EXCEPTION_FRAME, *PKI_EXCEPTION_FRAME;

VOID KiHandleException(PKI_EXCEPTION_FRAME ExceptionFrame);

#endif
