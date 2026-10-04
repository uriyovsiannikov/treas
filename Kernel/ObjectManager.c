#include <Treas/ObjectManager.h>
#include <Treas/SpinLock.h>
#include <Treas/Thread.h>

#define OB_MAXIMUM_HANDLE_COUNT 32
#define OB_HANDLE_BASE 0x100u
#define OB_OBJECT_TYPE_NONE 0u
#define OB_OBJECT_TYPE_EVENT 1u

typedef struct _OB_HANDLE_ENTRY {
    ULONG Type;
    ULONG GrantedAccess;
    BOOLEAN Signaled;
    BOOLEAN ManualReset;
} OB_HANDLE_ENTRY, *POB_HANDLE_ENTRY;

static OB_HANDLE_ENTRY ObpHandleTable[OB_MAXIMUM_HANDLE_COUNT];
static KSPIN_LOCK ObpHandleTableLock;

static POB_HANDLE_ENTRY ObpReferenceHandle(ULONG Handle,
                                          ULONG RequiredAccess)
{
    ULONG Index;
    POB_HANDLE_ENTRY Entry;

    if (Handle < OB_HANDLE_BASE) {
        return 0;
    }
    Index = Handle - OB_HANDLE_BASE;
    if (Index >= OB_MAXIMUM_HANDLE_COUNT) {
        return 0;
    }
    Entry = &ObpHandleTable[Index];
    if (Entry->Type == OB_OBJECT_TYPE_NONE ||
        (Entry->GrantedAccess & RequiredAccess) != RequiredAccess) {
        return 0;
    }
    return Entry;
}

static BOOLEAN ObpCreateEvent(PTREAS_OBJECT_REQUEST Request)
{
    ULONG Index;
    ULONG AllowedAccess = TREAS_EVENT_MODIFY_STATE | TREAS_SYNCHRONIZE;

    if (Request->DesiredAccess == 0 ||
        (Request->DesiredAccess & ~AllowedAccess) != 0 ||
        (Request->Flags & ~(TREAS_EVENT_MANUAL_RESET |
                            TREAS_EVENT_INITIAL_STATE)) != 0) {
        return FALSE;
    }
    for (Index = 0; Index < OB_MAXIMUM_HANDLE_COUNT; Index++) {
        if (ObpHandleTable[Index].Type == OB_OBJECT_TYPE_NONE) {
            ObpHandleTable[Index].Type = OB_OBJECT_TYPE_EVENT;
            ObpHandleTable[Index].GrantedAccess = Request->DesiredAccess;
            ObpHandleTable[Index].ManualReset =
                (Request->Flags & TREAS_EVENT_MANUAL_RESET) != 0;
            ObpHandleTable[Index].Signaled =
                (Request->Flags & TREAS_EVENT_INITIAL_STATE) != 0;
            Request->Handle = OB_HANDLE_BASE + Index;
            return TRUE;
        }
    }
    return FALSE;
}

BOOLEAN ObManageUserObject(PTREAS_OBJECT_REQUEST Request)
{
    BOOLEAN RestoreInterrupts;
    POB_HANDLE_ENTRY Entry;
    BOOLEAN Result = FALSE;

    if (Request == 0) {
        return FALSE;
    }
    if (Request->Operation == TREAS_OBJECT_OPERATION_WAIT) {
        for (;;) {
            RestoreInterrupts = KeAcquireSpinLock(&ObpHandleTableLock);
            Entry = ObpReferenceHandle(Request->Handle, TREAS_SYNCHRONIZE);
            if (Entry == 0) {
                KeReleaseSpinLock(&ObpHandleTableLock, RestoreInterrupts);
                return FALSE;
            }
            if (Entry->Signaled) {
                if (!Entry->ManualReset) {
                    Entry->Signaled = FALSE;
                }
                KeReleaseSpinLock(&ObpHandleTableLock, RestoreInterrupts);
                return TRUE;
            }
            KeReleaseSpinLock(&ObpHandleTableLock, RestoreInterrupts);
            KeYieldThread();
        }
    }

    RestoreInterrupts = KeAcquireSpinLock(&ObpHandleTableLock);
    if (Request->Operation == TREAS_OBJECT_OPERATION_CREATE_EVENT) {
        Result = ObpCreateEvent(Request);
    } else if (Request->Operation == TREAS_OBJECT_OPERATION_SET_EVENT) {
        Entry = ObpReferenceHandle(Request->Handle, TREAS_EVENT_MODIFY_STATE);
        if (Entry != 0 && Entry->Type == OB_OBJECT_TYPE_EVENT) {
            Entry->Signaled = TRUE;
            Result = TRUE;
        }
    } else if (Request->Operation == TREAS_OBJECT_OPERATION_RESET_EVENT) {
        Entry = ObpReferenceHandle(Request->Handle, TREAS_EVENT_MODIFY_STATE);
        if (Entry != 0 && Entry->Type == OB_OBJECT_TYPE_EVENT) {
            Entry->Signaled = FALSE;
            Result = TRUE;
        }
    } else if (Request->Operation == TREAS_OBJECT_OPERATION_CLOSE) {
        Entry = ObpReferenceHandle(Request->Handle, 0);
        if (Entry != 0) {
            Entry->Type = OB_OBJECT_TYPE_NONE;
            Result = TRUE;
        }
    }
    KeReleaseSpinLock(&ObpHandleTableLock, RestoreInterrupts);
    return Result;
}
