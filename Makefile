# ── Platform selection ─────────────────────────────────────────────
# Usage:  make               (defaults to x86)
#         make PLATFORM=x86
#         make PLATFORM=rpi3
#
# PLATFORM drives every aspect of the build: compiler, linker flags,
# source file selection, and the QEMU invocation command.
PLATFORM ?= x86

# Each platform gets its own build directory so that object files
# compiled for x86 (ELF32) cannot accidentally be linked into the
# AArch64 binary and vice versa.
BUILD = build/$(PLATFORM)

# ── x86 — 32-bit protected mode, Multiboot, QEMU PC ───────────────
ifeq ($(PLATFORM),x86)

CC      = gcc
LD      = ld

# -m32             : target i386 (32-bit) even on a 64-bit host
# -ffreestanding   : do not assume a hosted C library
# -nostdlib        : do not link the standard library
# -fno-builtin     : prevent compiler from replacing calls with built-ins
# -fno-stack-protector : no __stack_chk_fail (no libc to provide it)
# -fno-pic         : no position-independent code (kernel at fixed address)
CFLAGS  = -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
          -nostdlib -fno-builtin -fno-stack-protector \
          -fno-pic -Isrc

# -m elf_i386      : produce a 32-bit ELF output on a 64-bit host ld
# -T linker_x86.ld : use our custom linker script (kernel at 1 MB)
LDFLAGS = -m elf_i386 -T src/linker_x86.ld -nostdlib

OBJS  = $(BUILD)/boot_x86.o \
        $(BUILD)/vga.o       \
        $(BUILD)/keyboard.o  \
        $(BUILD)/sound.o     \
        $(BUILD)/shell.o     \
        $(BUILD)/kernel.o

TARGET = exigeos_x86.bin

# -kernel loads a Multiboot-compliant ELF or flat binary directly,
# bypassing the BIOS boot sector.  The audio flags route the PC
# speaker emulation through PipeWire.  Change 'pipewire' to 'pa' if
# your system uses PulseAudio without the PipeWire compatibility layer.
QEMU_CMD = qemu-system-i386 -kernel $(TARGET) \
               -audiodev pipewire,id=snd0 \
               -machine pc,pcspk-audiodev=snd0

# Capture PC speaker audio to a WAV file for offline inspection.
QEMU_WAV = qemu-system-i386 -kernel $(TARGET) \
               -audiodev wav,id=snd0,path=/tmp/exigeos.wav \
               -machine pc,pcspk-audiodev=snd0

# ── Raspberry Pi 3B — AArch64, QEMU raspi3b ───────────────────────
else ifeq ($(PLATFORM),rpi3)

# Cross-compiler required: sudo apt install gcc-aarch64-linux-gnu
CC      = aarch64-linux-gnu-gcc
LD      = aarch64-linux-gnu-ld

# -DPLATFORM_RPI3  : enables RPi3-specific code paths in shell.c, etc.
CFLAGS  = -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
          -nostdlib -fno-builtin -fno-stack-protector \
          -fno-pic -Isrc -DPLATFORM_RPI3

LDFLAGS = -T src/linker_rpi3.ld -nostdlib

OBJS  = $(BUILD)/boot_rpi3.o     \
        $(BUILD)/vga_rpi3.o      \
        $(BUILD)/keyboard_rpi3.o \
        $(BUILD)/sound_stub.o    \
        $(BUILD)/shell.o         \
        $(BUILD)/kernel.o

TARGET = exigeos_rpi3.elf

# -machine raspi3b : Raspberry Pi 3 Model B (BCM2837, 4× Cortex-A53)
# -serial stdio    : map the PL011 UART to the host terminal
# -display none    : no graphical display (serial only)
# -no-reboot       : stop QEMU instead of rebooting on reset
QEMU_CMD = qemu-system-aarch64 \
               -machine raspi3b   \
               -cpu cortex-a53    \
               -m 1G              \
               -kernel $(TARGET)  \
               -serial stdio      \
               -display none      \
               -no-reboot

QEMU_WAV = @echo "No PC speaker on RPi3 — audio not available"

else
$(error Unknown PLATFORM '$(PLATFORM)'. Use: make PLATFORM=x86  or  make PLATFORM=rpi3)
endif

# ── Common rules ──────────────────────────────────────────────────
.PHONY: all clean run run-wav

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

# Create the per-platform build directory on demand.
$(BUILD):
	mkdir -p $(BUILD)

# x86 boot stub: assembled with NASM into a 32-bit ELF object.
$(BUILD)/boot_x86.o: src/boot_x86.asm | $(BUILD)
	nasm -f elf32 -o $@ $<

# RPi3 boot stub: assembled via the AArch64 cross-compiler (GAS syntax).
$(BUILD)/boot_rpi3.o: src/boot_rpi3.S | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

# Generic C compilation rule (applies to all .c sources).
$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

run: $(TARGET)
	$(QEMU_CMD)

run-wav: $(TARGET)
	$(QEMU_WAV)

clean:
	rm -rf build/ exigeos_x86.bin exigeos_rpi3.elf
