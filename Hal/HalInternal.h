#ifndef _TREAS_HAL_INTERNAL_H_
#define _TREAS_HAL_INTERNAL_H_

#include <Treas/Types.h>

VOID HalpInitializePic(VOID);
VOID HalpSetTimerMasked(BOOLEAN Masked);
PVOID HalpMapSharedChannel(ULONGLONG ChannelSize);

#endif
