#ifndef _TREAS_HOST_FILE_PORTAL_H_
#define _TREAS_HOST_FILE_PORTAL_H_

#include <Treas/SharedProtocol.h>

#define TREASP_FILE_ACCESS_READ 1u
#define TREASP_FILE_ACCESS_WRITE 2u

typedef struct _TREASP_FILE_ARGUMENT {
    const char *Path;
    ULONG Access;
} TREASP_FILE_ARGUMENT, *PTREASP_FILE_ARGUMENT;

typedef struct _TREASP_HOST_FILE {
    int Descriptor;
    ULONG Access;
} TREASP_HOST_FILE, *PTREASP_HOST_FILE;

typedef struct _TREASP_FILE_PORTAL {
    TREASP_HOST_FILE Files[TREAS_SHARED_MAX_FILES];
    ULONG FileCount;
} TREASP_FILE_PORTAL, *PTREASP_FILE_PORTAL;

int TreaspInitializeFilePortal(PTREASP_FILE_PORTAL Portal,
                               ULONG FileCount,
                               const TREASP_FILE_ARGUMENT *Files);
void TreaspServiceFilePortal(PTREASP_FILE_PORTAL Portal,
                             PTREAS_SHARED_CHANNEL Channel);
void TreaspDestroyFilePortal(PTREASP_FILE_PORTAL Portal);

#endif
