#ifndef _TREAS_HOST_LAUNCH_IMAGE_H_
#define _TREAS_HOST_LAUNCH_IMAGE_H_

#include <Treas/Types.h>

int TreaspCreateLaunchImage(const char *BinaryPath,
                            ULONG ArgumentCount,
                            char *const *Arguments,
                            char **LaunchPath);

#endif
