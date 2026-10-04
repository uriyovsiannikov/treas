#ifndef _TREAS_TIMER_H_
#define _TREAS_TIMER_H_

#include <Treas/Types.h>

#define KI_SYSTEM_TIMER_FREQUENCY 100

VOID KiTimerInterrupt(VOID);
ULONGLONG KiQueryTimerTickCount(VOID);

#endif
