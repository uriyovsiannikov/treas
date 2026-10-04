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
#include "GuestMemory.h"
#include "VirtualMachine.h"

static VOID TreaspForwardPipedInput(int InputDescriptor)
{
    static const char EndOfInput = 0x04;
    struct timespec Delay = {0, 100000000};
    char Buffer[4096];

    while (nanosleep(&Delay, &Delay) < 0 && errno == EINTR) {
    }
    for (;;) {
        ssize_t Size = read(STDIN_FILENO, Buffer, sizeof(Buffer));

        if (Size < 0 && errno == EINTR) {
            continue;
        }
        if (Size <= 0 ||
            !TreaspWriteFile(InputDescriptor, Buffer, (size_t)Size)) {
            break;
        }
    }
    (void)TreaspWriteFile(InputDescriptor, &EndOfInput, 1);
    close(InputDescriptor);
}

int TreaspRunVirtualMachine(const char *KernelPath, const char *LaunchPath)
{
    char MemorySize[32];
    char ApplicationErrorPath[] = "/tmp/TreasApplicationErrorXXXXXX";
    char QemuErrorPath[] = "/tmp/TreasQemuErrorXXXXXX";
    char ErrorSerialArgument[sizeof(ApplicationErrorPath) + sizeof("file:")];
    const char *MachineType;
    const char *ProcessorType;
    ULONGLONG MemoryMegabytes;
    int ApplicationErrorDescriptor;
    int QemuErrorDescriptor;
    char *Arguments[] = {
        "qemu-system-x86_64",
        "-machine", NULL,
        "-cpu", NULL,
        "-m", MemorySize,
        "-kernel", (char *)KernelPath,
        "-initrd", (char *)LaunchPath,
        "-nodefaults",
        "-serial", "null",
        "-serial", "stdio",
        "-serial", ErrorSerialArgument,
        "-monitor", "none",
        "-display", "none",
        "-no-reboot",
        "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04",
        NULL
    };
    pid_t ChildProcess;
    int ChildStatus;
    int ApplicationStatus;
    int InputPipe[2] = {-1, -1};
    int ForwardInput = 0;
    struct stat InputStatus;
    struct stat QemuErrorStatus;

    ApplicationErrorDescriptor = mkstemp(ApplicationErrorPath);
    if (ApplicationErrorDescriptor < 0) {
        perror("treas: cannot create application error stream");
        return 1;
    }
    close(ApplicationErrorDescriptor);
    QemuErrorDescriptor = mkstemp(QemuErrorPath);
    if (QemuErrorDescriptor < 0) {
        perror("treas: cannot create QEMU error stream");
        unlink(ApplicationErrorPath);
        return 1;
    }
    snprintf(ErrorSerialArgument, sizeof(ErrorSerialArgument), "file:%s",
             ApplicationErrorPath);
    if (access("/dev/kvm", R_OK | W_OK) == 0) {
        MachineType = "q35,accel=kvm";
        ProcessorType = "host";
    } else {
        MachineType = "q35,accel=tcg";
        ProcessorType = "max";
    }
    Arguments[2] = (char *)MachineType;
    Arguments[4] = (char *)ProcessorType;
    if (fstat(STDIN_FILENO, &InputStatus) == 0 &&
        (S_ISFIFO(InputStatus.st_mode) || S_ISREG(InputStatus.st_mode))) {
        if (pipe(InputPipe) != 0) {
            perror("treas: cannot create application input stream");
            close(QemuErrorDescriptor);
            unlink(QemuErrorPath);
            unlink(ApplicationErrorPath);
            return 1;
        }
        ForwardInput = 1;
        (void)signal(SIGPIPE, SIG_IGN);
    }
    MemoryMegabytes = TreaspGetGuestMemoryMegabytes();
    snprintf(MemorySize, sizeof(MemorySize), "%lluM",
             (unsigned long long)MemoryMegabytes);
    ChildProcess = fork();
    if (ChildProcess < 0) {
        perror("treas: cannot start virtual machine");
        if (ForwardInput) {
            close(InputPipe[0]);
            close(InputPipe[1]);
        }
        close(QemuErrorDescriptor);
        unlink(QemuErrorPath);
        unlink(ApplicationErrorPath);
        return 1;
    }
    if (ChildProcess == 0) {
        if (ForwardInput) {
            close(InputPipe[1]);
            if (dup2(InputPipe[0], STDIN_FILENO) < 0) {
                _exit(126);
            }
            close(InputPipe[0]);
        }
        if (dup2(QemuErrorDescriptor, STDERR_FILENO) < 0) {
            _exit(126);
        }
        close(QemuErrorDescriptor);
        execvp(Arguments[0], Arguments);
        perror("treas: cannot execute QEMU");
        _exit(127);
    }
    close(QemuErrorDescriptor);
    if (ForwardInput) {
        close(InputPipe[0]);
        TreaspForwardPipedInput(InputPipe[1]);
    }

    while (waitpid(ChildProcess, &ChildStatus, 0) < 0) {
        if (errno == EINTR) {
            continue;
        }
        perror("treas: waiting for virtual machine failed");
        unlink(QemuErrorPath);
        unlink(ApplicationErrorPath);
        return 1;
    }
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
    TreaspCopyFileToDescriptor(ApplicationErrorPath, STDERR_FILENO);
    unlink(QemuErrorPath);
    unlink(ApplicationErrorPath);
    return ApplicationStatus;
}
