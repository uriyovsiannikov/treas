# Treas OS

Treas is a new x86_64 operating system project with an NT-inspired kernel design. It currently targets one boot path: QEMU's PVH direct loader. The kernel enters 64-bit long mode, reads the PVH start information, and keeps the launcher output reserved for the guest application.

## Execution model

An ordinary container shares the host kernel, so it cannot provide a second independent kernel. Treas runs as a virtual machine. On Linux, QEMU with KVM can execute guest CPU instructions directly on the processor. Device emulation and hypercalls still have overhead; Virtio is the planned fast I/O path. A container can package and launch the VM, but it does not replace virtualization.

## Build and run

Requirements: GCC, NASM, GNU ld, and QEMU (`qemu-system-x86_64`).

```sh
make
make test
export PATH="$PWD/Build:$PATH"
treas -b Build/testapp.texb
treas -b Build/testapp.texb -- alpha beta
treas -b Build/fetch.texb
```

The host launcher accepts `-b <file.texb>`. Every command creates a small temporary launch image containing the executable and its arguments, starts a fresh QEMU virtual machine, and passes that image through QEMU's PVH `-initrd` module. The VM exits as soon as the application exits. No background service, socket, or persistent guest session is involved. Guest RAM is selected on each launch from host-available memory and any detected cgroup limit: normally one quarter of the available amount, clamped to 64 MiB–3072 MiB. The upper bound matches the current physical-memory manager's below-4-GiB tracking limit. QEMU uses KVM with the host CPU model when `/dev/kvm` is available and otherwise uses TCG with its `max` CPU model. QEMU's default devices are disabled. Application stdout uses COM2 and goes directly to the launcher's stdout; application stderr uses COM3 and is forwarded to the launcher's stderr when the VM exits. Stdin is read from COM2. A read blocks until at least one byte is available, then returns currently buffered bytes; Ctrl+D signals EOF. Arguments after `--` are passed to the program; its `argv[0]` is the resolved `.texb` path. Up to 64 arguments and 16 KiB of combined argument text are supported. The command returns the program's exit status. Add `Build` to `PATH` to use the shorter form `treas -b Build/fetch.texb`. A `.texb` file currently contains a static ELF64 x86-64 executable. `Build/testapp.texb` and `Build/fetch.texb` are generated applications. `fetch.texb` displays the CPU brand reported by CPUID, Treas version, managed and free RAM, vCPU count, PVH runtime, and uptime.

## Native application SDK

`Include/Treas/UserApi.h` is the supported C interface for native applications.
`Include/Treas/UserAbi.h` versions the syscall ABI and publishes its stable
service numbers separately from kernel dispatch declarations. Link
`User/Runtime/Entry.asm` and `User/Runtime/NativeApi.c` with an application that
exports `LONG TreUserMain(ULONG ArgumentCount, const CHAR **Arguments)`.
Applications can read stdin, write stdout and stderr, query system information,
create or exit a thread, yield, and terminate the process through the SDK. Reads
block for the first byte and return up to the requested size; Ctrl+D reports EOF.
The SDK is freestanding
and uses the x86-64 System V C calling convention. `make test` builds and runs
`Tests/SdkTest.c` through this public interface.

QEMU uses KVM when the launcher can access `/dev/kvm` and uses TCG otherwise. The direct ELF load uses the Xen PVH physical 32-bit entry note; QEMU looks for this note when loading an uncompressed ELF kernel ([QEMU x86 loader](https://github.com/qemu/qemu/blob/master/hw/i386/x86-common.c)).

## Source layout

```text
Arch/X64/       CPU entry, exception stubs, and architecture-specific code
Build/          Host launcher and generated application
Build/Obj/      Generated objects and kernel image
Tests/          User-thread, preemption, and system-information syscall integration tests
Hal/            Hardware abstraction layer and interrupt-table setup
Host/           Host launcher and PVH launch-module builder
Include/Treas/  Shared kernel headers, PVH and launch protocols, and base types
Kernel/         Core startup, process startup, bugcheck, memory managers, and image loader
User/           Initial user program, Fetch utility modules, and ELF linker script
User/Runtime/   Native user ABI wrappers and the standard C application entry
Linker.ld       Kernel image layout
Makefile        Build and QEMU launch targets
```

## Code conventions

- Source files and directories use PascalCase.
- Public kernel routines use subsystem prefixes such as `Ki` for kernel internals and `Hal` for hardware abstraction routines. Private routines place a lowercase `p` after the subsystem prefix, as in `HalpWritePortByte`.
- Kernel interfaces use NT-style base types (`VOID`, `ULONG`, `BOOLEAN`) instead of compiler-specific integer names in routine signatures.
- Functions and control blocks use Allman braces. Locals and structure fields use PascalCase.

## Initial architecture

The planned layers are:

- **HAL:** interrupt controllers, timers, CPU topology, and platform devices.
- **Kernel:** scheduling, traps, synchronization, and low-level memory management.
- **Executive:** process and thread objects, handles, virtual memory, I/O manager, and security boundaries.
- **Drivers:** isolated device-facing modules, with Virtio as the initial device family.
- **User mode:** initial ring 3 process, static ELF loader, and native system-call entry are implemented. User threads share the process address space, have independent eight-page stacks, and can yield cooperatively; the PIT preempts ring 3 when another thread is ready.

The kernel currently installs IDT gates for CPU exception vectors 0 through 31 and the PIT timer at vector 32. An exception in the primary user thread terminates the guest application with status 127; an exception in a child user thread terminates only that thread. Kernel exceptions bugcheck on a dedicated stack. The 100 Hz PIT interrupt updates a kernel tick counter and acknowledges the PIC; it preempts a ring 3 thread only when another thread is ready. Kernel-mode timer interrupts only update the counter, keeping scheduler queue changes out of interruptible kernel code. Only IRQ0 is unmasked; other hardware interrupts remain disabled. The PVH memory map initializes a next-fit physical page allocator with one allocation bitmap and a compact list of permanently allocatable ranges. It tracks physical addresses below 4 GiB, which are identity-mapped with 2 MiB pages at boot. The virtual memory manager maps and unmaps 4 KiB user pages, including splitting an existing 2 MiB identity mapping when needed. It enables NX when supported and sets CR0.WP. Spin locks protect the allocator, page-table mutations, and user-thread stack slots. QEMU still starts with one vCPU: the scheduler's current-thread pointer and ready queue are global, so SMP stays disabled until scheduler state is per-CPU.

The kernel requires one PVH module containing the launch header, executable, and argument strings. It creates a separate address space for the ring 3 process and loads its ELF image. User PML4 slots are private; only the kernel's upper-half mappings and the required low identity mapping are shared. The loader accepts static ELF64 x86-64 `ET_EXEC` images with `PT_LOAD` segments; it validates program-header bounds, segment alignment, executable entry coverage, and non-overlapping pages, and rejects writable-executable mappings and reserved user-thread ranges. `.text`/`.rodata` are mapped RX and `.data`/`.bss` are mapped RW/NX; BSS pages are zero-filled. The current image limit is 256 pages. User image mappings can occupy the lower canonical range from 4 GiB up to 128 TiB, above the kernel's identity map. Process entry receives `argc` in `RDI` and a null-terminated `argv` vector in `RSI`; the strings and pointer vector are built on an eight-page user stack. Its native `SYSCALL`/`SYSRET` ABI puts the service number in `EAX`, arguments in `RDI` and `RSI`, and the result in `RAX`; volatile registers may be clobbered. Before `SYSRET`, Treas validates the user instruction pointer and stack pointer and sanitizes RFLAGS. Service 1 writes stdout, service 2 exits the process with a status from 0 through 127, service 3 yields cooperatively, service 4 creates a user thread (`RDI` entry point, `RSI` argument; returns a thread slot or `-2`), service 5 exits the calling non-primary user thread, service 6 fills a versioned system-information structure in a writable user buffer, service 7 reads stdin, and service 8 writes stderr. `fetch.texb` uses service 6 for managed/free RAM, timer ticks, timer frequency, and vCPU count; it reads the CPU brand with CPUID. A thread function uses the x86-64 System V calling convention and returns normally; a small RX thunk then invokes service 5. Child threads share the process address space and receive separate eight-page stacks. The fixed scheduler has eight slots: one bootstrap thread and up to seven created threads, including the primary process thread, so at most six child threads can run at once; each thread slot has a 4 KiB kernel stack. Timer preemption can switch away from ring 3 and later restore its saved interrupt frame; kernel-mode thread execution remains cooperative. Spin locks compile to inline interrupt exclusion in the current single-vCPU build, avoiding function calls and locked instructions; defining `TREAS_MULTIPROCESSOR` enables atomic locks for future SMP work. The host launcher returns the process status. Dynamic linking, PIE, multiple address spaces in the scheduler, and Windows API compatibility are not implemented.
