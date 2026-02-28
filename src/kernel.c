/*
 * kernel.c — Kernel entry point
 *
 * kernel_main() is called by the platform-specific boot stub after:
 *   x86 : the CPU is in 32-bit protected mode with a valid stack.
 *   RPi3: core 0 is in AArch64 mode, BSS is zeroed, stack is ready.
 *
 * Initialisation order matters:
 *   1. vga_init()      — set up the display first so subsequent steps
 *                        can print error messages if needed.
 *   2. keyboard_init() — prepare input before the shell loop starts.
 *   3. shell_run()     — enter the interactive loop (never returns).
 *
 * There is no memory allocator, no scheduler, and no interrupt handling
 * beyond the PS/2 polling in keyboard.c.  Everything runs sequentially
 * in a single infinite loop at ring 0 (x86) / EL1 or EL2 (AArch64).
 */

#include "vga.h"
#include "keyboard.h"
#include "shell.h"

void kernel_main(void) {
    vga_init();
    keyboard_init();

    vga_print("EXIGE OS [version 0.1]");
    vga_newline();

    shell_run();    /* never returns */
}
