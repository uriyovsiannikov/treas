#include <Treas/SpinLock.h>

#ifdef TREAS_MULTIPROCESSOR
VOID KeInitializeSpinLock(PKSPIN_LOCK SpinLock)
{
    SpinLock->LockWord = 0;
}

BOOLEAN KeAcquireSpinLock(PKSPIN_LOCK SpinLock)
{
    ULONGLONG Flags;

    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(Flags) : : "memory");
    while (__atomic_exchange_n(&SpinLock->LockWord, 1, __ATOMIC_ACQUIRE) != 0) {
        while (__atomic_load_n(&SpinLock->LockWord, __ATOMIC_RELAXED) != 0) {
            __asm__ volatile ("pause");
        }
    }

    return (BOOLEAN)((Flags & (1ULL << 9)) != 0);
}

VOID KeReleaseSpinLock(PKSPIN_LOCK SpinLock, BOOLEAN RestoreInterrupts)
{
    __atomic_store_n(&SpinLock->LockWord, 0, __ATOMIC_RELEASE);
    if (RestoreInterrupts) {
        __asm__ volatile ("sti" : : : "memory");
    }
}
#endif
