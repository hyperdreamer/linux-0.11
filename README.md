# Linux 0.11 — A Modernized Build of the Classic Kernel

This repository is a buildable, lightly modernized version of the original
**Linux 0.11** kernel — the early 1991 release by Linus Torvalds that helped
launch the entire Linux project. The source has been adapted so it compiles
with a contemporary GNU toolchain (GNU `as`/`ld`, 32-bit `gcc`) and runs under
modern emulators such as **QEMU** and **Bochs**.

The goal of the project is learning and experimentation: the codebase is small
enough to read end-to-end, yet it is a complete, self-hosting Unix-like kernel
with process scheduling, virtual memory, a MINIX filesystem, block and character
device drivers, and a system-call interface.

## Historical Significance

Linux 0.11 is one of the most studied operating-system code bases in existence.
Released in December 1991, it is the first version that could compile itself and
was widely used as a teaching kernel (notably in books on the early Linux source).
It is compact — a few thousand lines of C and assembly — but already contains the
architectural ideas that grew into the modern kernel: a monolithic design,
preemptive multitasking on the i386, demand paging with copy-on-write, and a
POSIX-style system-call layer. Reading it is the closest you can get to seeing
how Linux began.

## What's Different in This Build

This is not a byte-for-byte copy of the 1991 tarball. The notable adaptations are:

- **Modern GNU assembler syntax.** The boot and low-level sources (`boot/*.s`,
  `kernel/*.s`, `kernel/chr_drv/*.s`, `mm/page.s`) use AT&T/GAS syntax and are
  assembled with `as --32` rather than the original 8086/`as86` + `gas` mix.
- **ELF intermediate objects, linked to a flat binary.** Components are linked
  with `ld -m elf_i386` using the linker scripts `kernel.ld` and `boot/boot.ld`,
  then converted to raw binary with `objcopy`.
- **A built-in debug switch.** `include/linux/kernel.h` defines a `breakpoint()`
  macro that emits Bochs's "magic breakpoint" instruction (`xchgw %bx, %bx`) and
  a `printkc()` colored-print helper. These are gated on the `DEBUG` macro and
  can be toggled in `BUILD_CONFIG.mk` (see [Debugging](#debugging)).
- **Centralized build configuration.** Toolchain and flags live in
  `BUILD_CONFIG.mk`, included by every `Makefile`, so the whole tree builds with
  one consistent set of options.
- **21st-century clock handling.** `init/main.c` adjusts the CMOS-derived year so
  the system clock reads correctly on present-day hosts.

The author's notes in the source credit the original code to Linus Torvalds (1991)
with modernization work attributed to "He Wen" / Henry.

## Repository Layout

```
linux-0.11/
├── boot/              Real-mode boot path (16-bit) and protected-mode entry
│   ├── bootsect.s     Boot sector: loaded at 0x7C00, relocates to 0x90000,
│   │                  loads setup + system, picks root device, jumps to setup
│   ├── setup.s        Gathers BIOS data (memory, video, disks), enters pmode
│   ├── head.s         32-bit entry: sets up GDT/IDT/paging, calls main()
│   └── boot.ld        Linker script for bootsect/setup (.text at 0x0)
├── init/
│   └── main.c         Kernel C entry point: subsystem init, then forks init
├── kernel/            Core kernel: scheduling, syscalls, traps, signals
│   ├── sched.c        The scheduler and timer handling
│   ├── system_call.s  System-call entry / dispatch
│   ├── fork.c         Process creation (copy-on-write setup)
│   ├── exit.c         Process termination and reaping
│   ├── signal.c       Signal delivery
│   ├── traps.c        CPU exception/trap handlers
│   ├── sys.c          Miscellaneous system calls
│   ├── printk.c       vsprintf.c  panic.c  mktime.c  asm.s
│   ├── blk_drv/       Block device drivers (hd, floppy, ramdisk, ll_rw_blk)
│   ├── chr_drv/       Character devices (console, tty, serial, keyboard)
│   └── math/          Software FPU emulation (math_emulate.c)
├── mm/                Memory management
│   ├── memory.c       Paging, page faults, copy-on-write, free-page mgmt
│   └── page.s         Low-level page-fault entry
├── fs/                MINIX filesystem and VFS-style layer
│   ├── buffer.c       Block buffer cache
│   ├── inode.c  super.c  namei.c  bitmap.c  truncate.c
│   ├── open.c   read_write.c  exec.c  stat.c  fcntl.c  ioctl.c
│   ├── file_dev.c  block_dev.c  char_dev.c  pipe.c  file_table.c
├── lib/               Minimal C library used by the kernel's user-space stubs
│   ├── string.c  malloc.c  ctype.c  errno.c
│   └── open.c  close.c  dup.c  execve.c  _exit.c  wait.c  write.c  setsid.c
├── include/           Kernel headers
│   ├── linux/         sched.h, fs.h, mm.h, head.h, tty.h, kernel.h, ...
│   ├── asm/           system.h, io.h, segment.h, memory.h
│   ├── sys/           types.h, stat.h, times.h, wait.h, utsname.h
│   └── <libc-style>   unistd.h, fcntl.h, signal.h, string.h, time.h, ...
├── tools/             Build intermediates (system, kernel) — see Build
├── Makefile           Top-level build orchestration
├── BUILD_CONFIG.mk    Toolchain + compiler/assembler/linker flags
├── kernel.ld          Linker script for the final kernel image
└── boot.img           Built bootable kernel image (output artifact)
```

> Note: object files (`*.o`, `*.a`), `System.map`, `tags`, `boot.img`, the
> `tools/system` / `tools/kernel` intermediates, and `include/debug.h` are build
> outputs and are listed in `.gitignore`.

## Build

### Toolchain Requirements

The exact toolchain is defined in `BUILD_CONFIG.mk`. You need a GNU/Linux
environment (tested on Ubuntu 22.04 and Arch Linux) with 32-bit build support:

- **GCC with 32-bit multilib** — the build uses `gcc -m32 -march=pentium3`.
- **GNU binutils** providing `as`, `ld`, `ar`, `objcopy`, `strip` —
  `as --32` and `ld -m elf_i386` are used for 32-bit/ELF output.
- **Exuberant/Universal Ctags** at `/usr/bin/ctags` — used by the `tags` target.
  (The build's final `all` target depends on `tags`; if you don't have ctags,
  build the image target directly, see below.)
- **make**, `dd`, and `sync` (standard coreutils).

On Debian/Ubuntu the multilib support typically comes from
`gcc-multilib`; on Arch the base `gcc` already provides `-m32` with
`lib32-*` runtimes available if needed.

The relevant flags from `BUILD_CONFIG.mk`:

```make
AS      := as --32
CC      := gcc -m32 -march=pentium3
CFLAGS  := -O0 -fstrength-reduce -fomit-frame-pointer -finline-functions \
           -fno-stack-protector -nostdinc -fno-builtin
CPP     := cpp -nostdinc
LD      := ld -m elf_i386
AR      := ar
# Debug build (default):
CFLAGS  += -g -DDEBUG=1
```

### Building the Image

From the repository root:

```sh
make
```

This runs the default `all` target, which:

1. Recurses into `kernel/`, `mm/`, `fs/`, `kernel/blk_drv/`, `kernel/chr_drv/`,
   `kernel/math/`, and `lib/` to build their archives (`*.o` / `*.a`).
2. Links all components with `kernel.ld` into `tools/system`, writing the symbol
   map to `System.map`.
3. Strips and `objcopy`s `tools/system` into the flat binary `tools/kernel`.
4. Assembles and links `boot/bootsect` and `boot/setup` via `boot/boot.ld`.
5. Assembles `boot/head.o`.
6. Produces the bootable **`boot.img`** by concatenating, with `dd`:
   - `boot/bootsect` → sector 0 (512 bytes)
   - `boot/setup`    → sectors 1–4 (4 × 512 bytes)
   - `tools/kernel`  → sector 5 onward
7. Regenerates the `tags` file.

If you do not have `ctags` installed, you can build just the image without the
tags step:

```sh
make boot.img
```

### Cleaning

```sh
make clean
```

Removes `boot.img`, `System.map`, the boot/tool intermediates, and all object
files across the subdirectories.

### Optional: RAM Disk

A RAM disk device is available but disabled by default. To enable it, set the
`RAMDISK` size (in blocks) in the top-level `Makefile`:

```make
RAMDISK := -DRAMDISK=512
```

## Run

`make` produces **`boot.img`**, a floppy-style disk image whose layout is the
classic Linux 0.11 boot sequence:

| Offset (sectors) | Contents              |
|------------------|-----------------------|
| 0                | boot sector (`bootsect`) |
| 1–4              | `setup`               |
| 5 …              | kernel (`tools/kernel`) |

The boot sector ends with the `0xAA55` signature and selects a default root
device of `0x301` (first partition of the first hard disk; see the `ROOT_DEV`
constant in `boot/bootsect.s`). The image itself contains **only the kernel**, not
a root filesystem — booting to a shell (`/bin/sh` via `init`) requires a separate
MINIX root-filesystem disk image matching the configured root device.

### QEMU

Boot the kernel image as a floppy:

```sh
qemu-system-i386 -boot a -fda boot.img
```

To reach userspace (the kernel forks `init`, which runs `/etc/rc` then a login
shell), attach a Linux-0.11 MINIX root filesystem image as the hard disk so the
root device `0x301` resolves:

```sh
qemu-system-i386 -boot a -fda boot.img -hda rootfs.img
```

(`rootfs.img` is not part of this repository; supply a compatible Linux 0.11
root image, or adjust `ROOT_DEV` in `boot/bootsect.s` to match your setup.)

### Bochs

The kernel also runs under Bochs, which is the recommended environment for
debugging because of the magic-breakpoint support (see below). Point a Bochs
configuration at `boot.img` as the boot floppy and, if you have one, a root
filesystem image as the disk. (No `bochsrc` is bundled in this repository — add
one pointing `floppya` at `boot.img`.)

## Debugging

A small debug facility is built into the kernel via `include/linux/kernel.h`,
controlled by the `DEBUG` macro that is defined in `BUILD_CONFIG.mk`
(`CFLAGS += -DDEBUG=1`).

When `DEBUG` is enabled:

- `breakpoint()` emits the Bochs **magic breakpoint** instruction
  (`xchgw %bx, %bx`). With Bochs configured to honor magic breakpoints, hitting
  this drops you into the Bochs debugger at that exact point in the kernel.
- `printkc()` is available as a colored variant of `printk()` for trace output.

When `DEBUG` is disabled, `printkc()` compiles down to a no-op inline function
and the breakpoint macro is omitted, so the compiler optimizes the instrumentation
away with no source changes required.

To build a non-debug kernel, remove the debug additions at the bottom of
`BUILD_CONFIG.mk` (the `-g -DDEBUG=1` line).

## License

The original Linux 0.11 sources are © 1991 Linus Torvalds. This repository is a
derivative work intended for study and experimentation.
```