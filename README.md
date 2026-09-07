# Komodo

A Linux-compatible monolithic kernel for x86-64, written from scratch in C.

Komodo is part of the **EntoriNext** project. It boots through the
[Limine](https://limine-bootloader.org/) bootloader (UEFI and Legacy), brings up
all cores via SMP, and exposes a **Linux-compatible syscall ABI** (Linux 6.12
x86-64 numbering, syscalls 0–462). Unimplemented syscalls return `-ENOSYS`.

The kernel is development-focused: an EEVDF scheduler, page cache with swap, a
VFS with multiple filesystems, an in-house TCP/IP stack, and a Linux-style ABI
surface that grows with the project.

---

## Status

The boot path is verified: the kernel boots a BusyBox initramfs shell over
serial in QEMU. Userspace is layered on via `initramfs.cpio` in the EntoriNext
root, and the bootable ISO is produced at the EntoriNext root (not here). This
remains an experimental kernel — GPU, VirtIO, audio, and parts of the ABI may be
incomplete.

## Features

**Scheduling & processes**
- EEVDF scheduler with per-CPU runqueues and a red-black tree timeline
- SMP task placement, CPU migration, IPI-based preemption
- Priority Inheritance for robust futex/mutex semantics
- Kernel threads, user processes, VMA-backed memory, fd tables, credentials
- `ptrace`, cgroups with a pids controller

**Memory**
- Frame allocator, 4-level paging (4 KiB / 2 MiB / 1 GiB)
- Higher-half direct map (HHDM), buddy heap/slab
- Unified page cache with LRU reclaim, dirty writeback, readahead
- Swap subsystem for anonymous memory

**VFS & filesystems**
- UNIX-style VFS with mount points and callback drivers
- tmpfs, procfs, sysfs, devtmpfs, cgroupfs, cpio
- FAT12/16/32, exFAT, ext2/3/4, NTFS (write), ISO 9660 (Rock Ridge)

**Networking**
- Ethernet, ARP, IPv4/IPv6, ICMP/ICMPv6, NDP, UDP, TCP
- e1000/e1000e, RTL8139, RTL8169 NIC drivers
- `AF_INET`/`AF_INET6`, DHCP client, `/proc/net` views

**ABI & IPC**
- Linux x86-64 syscall ABI (0–462)
- `AF_UNIX`, `AF_NETLINK`, `AF_INET`, `AF_INET6`
- pipes, `epoll`, `eventfd`, `timerfd`, `signalfd`, `memfd`, `pidfd`,
  POSIX message queues, System V IPC
- futex + futex2, `mmap`/`munmap`/`mremap`, inotify
- POSIX termios, Unix98 PTYs, virtual terminals
- loadable modules (`init_module`/`finit_module`/`delete_module`)

**Security & tracing**
- seccomp filters with `no_new_privs`, user notifications, syscall stops

**Drivers**
- Input: PS/2, evdev, USB HID
- Storage: IDE/ATA, AHCI, NVMe, USB Mass Storage
- Audio: SB16, Intel HD Audio, ALSA ABI
- Display: DRM/KMS, VirtIO-GPU
- Bus: PCI/PCIe, USB host controllers, I2C
- Platform: ACPI, HPET, RTC, serial, TPM

## Architecture

```
Limine (UEFI / Legacy)
        |
        v
+--------------------+     +-------------------------+
| Early init         | --> | Platform & drivers      |
| FPU/SSE -> alloc   |     | ACPI -> SMP -> PCI      |
| paging -> heap     |     | storage/net/audio/USB   |
+--------------------+     +-------------------------+
        |                            |
        v                            v
+--------------------+     +-------------------------+
| VFS & filesystems  |     | Kernel services         |
| tmpfs/procfs/sysfs |     | scheduler -> IPC        |
| ext/NTFS/ISO9660   |     | syscalls -> signals     |
+--------------------+     +-------------------------+
        |                            |
        +------------+---------------+
                     |
                     v
          sched_start() -> init (PID 1)
```

## Getting Started

Build a kernel image:

```bash
make                  # produces KomImage
```

Run the kernel with an initramfs in QEMU:

```bash
make run
```

> ISO production (the bootable `Komodo-x64.iso`) lives at the **EntoriNext
> root**, where the `limine/` and `busybox/` submodules are checked out. See the
> root `Makefile` to build an ISO.

### Prerequisites

- `make`, `gcc`, `qemu-system`, `xorriso`, `clang-format`, `clang-tidy`,
  `kconfig-frontends`, `libncurses-dev`, `dos2unix`
- A `.config` (default: `.config-default`). Subsystems disabled by default
  (e.g. `VIRTIO`, `SOUND_SB16`) can be enabled with `make menuconfig`.

### Repo layout

```
Komodo/       this kernel
Userspace/    initramfs source (BusyBox)
limine/       bootloader submodule (upstream)
busybox/      busybox submodule (upstream)
```