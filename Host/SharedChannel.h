#ifndef _TREAS_HOST_SHARED_CHANNEL_H_
#define _TREAS_HOST_SHARED_CHANNEL_H_

#include <Treas/SharedProtocol.h>
#include "FilePortal.h"

typedef struct _TREASP_SHARED_CHANNEL {
    int Descriptor;
    char Path[64];
    PTREAS_SHARED_CHANNEL Header;
    int InputClosed;
} TREASP_SHARED_CHANNEL, *PTREASP_SHARED_CHANNEL;

int TreaspCreateSharedChannel(PTREASP_SHARED_CHANNEL Channel);
int TreaspPumpSharedChannel(PTREASP_SHARED_CHANNEL Channel,
                            PTREASP_FILE_PORTAL FilePortal);
void TreaspDestroySharedChannel(PTREASP_SHARED_CHANNEL Channel);

#endif
