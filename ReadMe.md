<div align="center">

# Treas OS

**A compact NT-inspired x86_64 operating system for isolated native applications.**

![Build](https://img.shields.io/badge/build-passing-2ea44f?style=for-the-badge)
![Architecture](https://img.shields.io/badge/arch-x86__64-0078d4?style=for-the-badge)
![ABI](https://img.shields.io/badge/syscall_ABI-v2-6f42c1?style=for-the-badge)
![Runtime](https://img.shields.io/badge/runtime-QEMU_%7C_KVM-f5a623?style=for-the-badge)
![License](https://img.shields.io/badge/license-MIT-yellow?style=for-the-badge)

<img src="Other/Screenshot1.png" alt="Treas OS fetch output" width="100%">

</div>

Treas starts a fresh virtual machine for one `.texb` application, streams its output directly to the host terminal, returns its exit code, and shuts down. KVM is selected automatically when available; otherwise QEMU TCG is used.

## Quick start

Requires GCC, NASM, GNU ld, Python 3, and `qemu-system-x86_64`.

```sh
make all
export PATH="$PWD/Build:$PATH"

treas -b Build/fetch.texb
treas -b Build/testapp.texb -- argument
```

Only application output is written to stdout and stderr. Host files can be explicitly preopened for the guest:

```sh
treas -b Build/app.texb --read Input.bin --write Output.bin
```

## Included

| Kernel | Runtime |
|---|---|
| Ring 3 processes and threads | Freestanding C SDK |
| 4 KiB virtual memory with NX and W^X | 64 KiB arena heap |
| NT-style processes, handles, and events | stdin, stdout, and stderr |
| TSC timekeeping and on-demand PIT | Preopened host files |
| Shared-memory `ivshmem` transport | 11 native system calls |

## Development

```sh
make test       # integration and protection tests
make benchmark  # VM startup latency
make clean
```

The code uses PascalCase paths, NT-style subsystem prefixes, small subsystem-specific files, and a versioned public ABI in [`Include/Treas/UserApi.h`](Include/Treas/UserApi.h).

Released under the [MIT License](LICENSE).
