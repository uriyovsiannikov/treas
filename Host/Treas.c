#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <Treas/LaunchProtocol.h>
#include "FilePortal.h"
#include "LaunchImage.h"
#include "VirtualMachine.h"

static int TreaspBuildKernelPath(char *KernelPath, size_t KernelPathSize)
{
    char ExecutablePath[PATH_MAX];
    char *DirectoryEnd;
    ssize_t PathLength;
    int DirectoryLength;
    int Result;

    PathLength = readlink("/proc/self/exe", ExecutablePath,
                          sizeof(ExecutablePath) - 1);
    if (PathLength < 0 || (size_t)PathLength >= sizeof(ExecutablePath) - 1) {
        perror("treas: cannot locate launcher executable");
        return 0;
    }
    ExecutablePath[PathLength] = '\0';

    DirectoryEnd = strrchr(ExecutablePath, '/');
    if (DirectoryEnd == NULL) {
        fprintf(stderr, "treas: launcher path has no directory\n");
        return 0;
    }
    DirectoryLength = (int)(DirectoryEnd - ExecutablePath);
    Result = snprintf(KernelPath, KernelPathSize, "%.*s/Obj/Treas.elf",
                      DirectoryLength, ExecutablePath);
    if (Result < 0 || (size_t)Result >= KernelPathSize) {
        fprintf(stderr, "treas: kernel path is too long\n");
        return 0;
    }
    return 1;
}

int main(int ArgumentCount, char **Arguments)
{
    char KernelPath[PATH_MAX];
    char ResolvedKernelPath[PATH_MAX];
    char *BinaryPath;
    size_t BinaryPathLength;
    int ApplicationArgumentStart;
    ULONG ApplicationArgumentCount;
    TREASP_FILE_ARGUMENT Files[TREAS_SHARED_MAX_FILES];
    ULONG FileCount;
    int ExitCode;

    if (ArgumentCount < 3 || strcmp(Arguments[1], "-b") != 0) {
        fprintf(stderr, "Usage: treas -b <application.texb> [--read file | --write file]... [-- arguments...]\n");
        return 2;
    }

    ApplicationArgumentStart = 3;
    FileCount = 0;
    while (ApplicationArgumentStart < ArgumentCount &&
           strcmp(Arguments[ApplicationArgumentStart], "--") != 0) {
        ULONG Access;

        if (strcmp(Arguments[ApplicationArgumentStart], "--read") == 0) {
            Access = TREASP_FILE_ACCESS_READ;
        } else if (strcmp(Arguments[ApplicationArgumentStart], "--write") == 0) {
            Access = TREASP_FILE_ACCESS_WRITE;
        } else {
            fprintf(stderr, "treas: expected --read, --write, or --\n");
            return 2;
        }
        if (ApplicationArgumentStart + 1 >= ArgumentCount ||
            FileCount == TREAS_SHARED_MAX_FILES) {
            fprintf(stderr, "treas: invalid host file arguments\n");
            return 2;
        }
        Files[FileCount].Access = Access;
        Files[FileCount].Path = Arguments[ApplicationArgumentStart + 1];
        FileCount++;
        ApplicationArgumentStart += 2;
    }
    if (ApplicationArgumentStart < ArgumentCount) {
        ApplicationArgumentStart++;
    }
    ApplicationArgumentCount = 1 +
        (ULONG)(ArgumentCount - ApplicationArgumentStart);

    BinaryPathLength = strlen(Arguments[2]);
    if (BinaryPathLength < 5 ||
        strcmp(Arguments[2] + BinaryPathLength - 5, ".texb") != 0) {
        fprintf(stderr, "treas: application file must use the .texb extension\n");
        return 2;
    }

    BinaryPath = realpath(Arguments[2], NULL);
    if (BinaryPath == NULL) {
        perror("treas: cannot open application binary");
        return 2;
    }
    if (!TreaspBuildKernelPath(KernelPath, sizeof(KernelPath)) ||
        realpath(KernelPath, ResolvedKernelPath) == NULL) {
        fprintf(stderr, "treas: kernel image not found at Build/Obj/Treas.elf\n");
        free(BinaryPath);
        return 2;
    }

    {
        char *ApplicationArguments[TREAS_LAUNCH_MAX_ARGUMENT_COUNT];
        char *LaunchPath;
        ULONG Index;

        if (ApplicationArgumentCount > TREAS_LAUNCH_MAX_ARGUMENT_COUNT) {
            fprintf(stderr, "treas: too many application arguments\n");
            free(BinaryPath);
            return 2;
        }
        ApplicationArguments[0] = BinaryPath;
        for (Index = 1; Index < ApplicationArgumentCount; Index++) {
            ApplicationArguments[Index] =
                Arguments[ApplicationArgumentStart + Index - 1];
        }

        if (!TreaspCreateLaunchImage(BinaryPath, ApplicationArgumentCount,
                                     ApplicationArguments, &LaunchPath)) {
            free(BinaryPath);
            return 2;
        }
        ExitCode = TreaspRunVirtualMachine(ResolvedKernelPath, LaunchPath,
                                           FileCount, Files);
        unlink(LaunchPath);
        free(LaunchPath);
    }
    free(BinaryPath);
    return ExitCode;
}
