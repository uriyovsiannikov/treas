#ifndef _TREAS_SPIN_LOCK_H_
#define _TREAS_SPIN_LOCK_H_

#include <Treas/Types.h>

typedef struct _KSPIN_LOCK {
#ifdef TREAS_MULTIPROCESSOR
    volatile ULONG LockWord;
#else
    UCHAR Reserved;
#endif
} KSPIN_LOCK, *PKSPIN_LOCK;

#ifdef TREAS_MULTIPROCESSOR
VOID KeInitializeSpinLock(PKSPIN_LOCK SpinLock);
BOOLEAN KeAcquireSpinLock(PKSPIN_LOCK SpinLock);
VOID KeReleaseSpinLock(PKSPIN_LOCK SpinLock, BOOLEAN RestoreInterrupts);
#else
static inline VOID KeInitializeSpinLock(PKSPIN_LOCK SpinLock)
{
    (void)SpinLock;
}

static inline BOOLEAN KeAcquireSpinLock(PKSPIN_LOCK SpinLock)
{
    ULONGLONG Flags;

    (void)SpinLock;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(Flags) : : "memory");
    return (BOOLEAN)((Flags & (1ULL << 9)) != 0);
}

static inline VOID KeReleaseSpinLock(PKSPIN_LOCK SpinLock,
                                     BOOLEAN RestoreInterrupts)
{
    (void)SpinLock;
    if (RestoreInterrupts) {
        __asm__ volatile ("sti" : : : "memory");
    }
}
#endif

#endif
