#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <Treas/Types.h>
#include "FileIo.h"
#include "FilePortal.h"
#include "GuestMemory.h"
#include "SharedChannel.h"
#include "VirtualMachine.h"

int TreaspRunVirtualMachine(const char *KernelPath,
                            const char *LaunchPath,
                            ULONG FileCount,
                            const TREASP_FILE_ARGUMENT *Files)
{
    char MemorySize[32];
    char QemuErrorPath[] = "/tmp/TreasQemuErrorXXXXXX";
    char SharedMemoryArgument[160];
    const char *MachineType;
    const char *ProcessorType;
    ULONGLONG MemoryMegabytes;
    int QemuErrorDescriptor;
    TREASP_SHARED_CHANNEL SharedChannel;
    TREASP_FILE_PORTAL FilePortal;
    char *Arguments[] = {
        "qemu-system-x86_64",
        "-machine", NULL,
        "-cpu", NULL,
        "-m", MemorySize,
        "-kernel", (char *)KernelPath,
        "-initrd", (char *)LaunchPath,
        "-nodefaults",
        "-object", SharedMemoryArgument,
        "-device", "ivshmem-plain,memdev=treas-shared",
        "-monitor", "none",
        "-display", "none",
        "-no-reboot",
        "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04",
        NULL
    };
    pid_t ChildProcess;
    int ChildStatus = 1 << 8;
    int ApplicationStatus;
    struct stat QemuErrorStatus;
    struct timespec PollDelay = {0, 1000000};

    if (!TreaspCreateSharedChannel(&SharedChannel)) {
        return 1;
    }
    if (!TreaspInitializeFilePortal(&FilePortal, FileCount, Files)) {
        TreaspDestroySharedChannel(&SharedChannel);
        return 1;
    }
    QemuErrorDescriptor = mkstemp(QemuErrorPath);
    if (QemuErrorDescriptor < 0) {
        perror("treas: cannot create QEMU error stream");
        TreaspDestroyFilePortal(&FilePortal);
        TreaspDestroySharedChannel(&SharedChannel);
        return 1;
    }
    snprintf(SharedMemoryArgument, sizeof(SharedMemoryArgument),
             "memory-backend-file,id=treas-shared,size=%u,share=on,mem-path=%s",
             TREAS_SHARED_CHANNEL_SIZE, SharedChannel.Path);
    if (access("/dev/kvm", R_OK | W_OK) == 0) {
        MachineType = "q35,accel=kvm";
        ProcessorType = "host";
    } else {
        MachineType = "q35,accel=tcg";
        ProcessorType = "max";
    }
    Arguments[2] = (char *)MachineType;
    Arguments[4] = (char *)ProcessorType;
    MemoryMegabytes = TreaspGetGuestMemoryMegabytes();
    snprintf(MemorySize, sizeof(MemorySize), "%lluM",
             (unsigned long long)MemoryMegabytes);
    ChildProcess = fork();
    if (ChildProcess < 0) {
        perror("treas: cannot start virtual machine");
        close(QemuErrorDescriptor);
        unlink(QemuErrorPath);
        TreaspDestroyFilePortal(&FilePortal);
        TreaspDestroySharedChannel(&SharedChannel);
        return 1;
    }
    if (ChildProcess == 0) {
        if (dup2(QemuErrorDescriptor, STDERR_FILENO) < 0) {
            _exit(126);
        }
        close(QemuErrorDescriptor);
        execvp(Arguments[0], Arguments);
        perror("treas: cannot execute QEMU");
        _exit(127);
    }
    close(QemuErrorDescriptor);
    (void)signal(SIGPIPE, SIG_IGN);
    for (;;) {
        pid_t WaitResult;

        if (!TreaspPumpSharedChannel(&SharedChannel, &FilePortal)) {
            (void)kill(ChildProcess, SIGTERM);
            while (waitpid(ChildProcess, &ChildStatus, 0) < 0 &&
                   errno == EINTR) {
            }
            break;
        }
        WaitResult = waitpid(ChildProcess, &ChildStatus, WNOHANG);
        if (WaitResult == ChildProcess) {
            break;
        }
        if (WaitResult < 0 && errno != EINTR) {
            perror("treas: waiting for virtual machine failed");
            ChildStatus = 1 << 8;
            break;
        }
        (void)nanosleep(&PollDelay, NULL);
    }
    (void)TreaspPumpSharedChannel(&SharedChannel, &FilePortal);
    if (WIFEXITED(ChildStatus) &&
        (WEXITSTATUS(ChildStatus) == 1 ||
         WEXITSTATUS(ChildStatus) == 127) &&
        stat(QemuErrorPath, &QemuErrorStatus) == 0 &&
        QemuErrorStatus.st_size != 0) {
        TreaspCopyFileToDescriptor(QemuErrorPath, STDERR_FILENO);
        ApplicationStatus = 1;
    } else if (WIFEXITED(ChildStatus)) {
        int ExitCode = WEXITSTATUS(ChildStatus);
        ApplicationStatus = (ExitCode & 1) ? (ExitCode - 1) / 2 : ExitCode;
    } else if (WIFSIGNALED(ChildStatus)) {
        ApplicationStatus = 128 + WTERMSIG(ChildStatus);
    } else {
        ApplicationStatus = 1;
    }
    unlink(QemuErrorPath);
    TreaspDestroyFilePortal(&FilePortal);
    TreaspDestroySharedChannel(&SharedChannel);
    return ApplicationStatus;
}
