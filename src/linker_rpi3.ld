/*
 * linker_rpi3.ld — Linker script for ExigeOS on Raspberry Pi 3B (AArch64)
 *
 * WHY LOAD AT 0x80000?
 * ----------------------
 * The Raspberry Pi GPU (VideoCore IV) reads kernel8.img from the SD card
 * and places it at physical address 0x80000 before releasing the ARM cores.
 * (The "8" in kernel8.img means AArch64 / 64-bit mode.)
 * This is a hardware convention: we must match it in the linker script.
 *
 * SECTION ORDER
 * --------------
 * .text.boot must come first because the CPU starts executing from
 * exactly 0x80000 — the very first instruction of _start in boot_rpi3.S.
 * If .text.boot were not first, _start might end up at an arbitrary
 * address and the CPU would execute garbage on reset.
 *
 * BSS SYMBOLS
 * ------------
 * __bss_start and __bss_end are *linker-defined* symbols.  They do not
 * exist as C variables; they are just addresses the linker computes from
 * the layout of the .bss section.  boot_rpi3.S uses them to zero the BSS
 * before calling kernel_main().
 * This manual zeroing is necessary because there is no ELF runtime loader
 * on bare metal to do it automatically.
 */

ENTRY(_start)

SECTIONS
{
    . = 0x80000;    /* GPU places kernel8.img here */

    /* Boot entry point must be the very first code in the binary. */
    .text.boot : { *(.text.boot) }

    .text : { *(.text .text.*) }

    .rodata : ALIGN(8) { *(.rodata .rodata.*) }

    .data : ALIGN(8) { *(.data .data.*) }

    /* Export BSS boundaries for the boot stub's zeroing loop. */
    . = ALIGN(8);
    __bss_start = .;
    .bss : { *(.bss .bss.*) *(COMMON) }
    . = ALIGN(8);
    __bss_end = .;
}
