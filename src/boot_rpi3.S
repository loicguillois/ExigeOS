/*
 * boot_rpi3.S — AArch64 entry point for ExigeOS on Raspberry Pi 3B
 *
 * HOW THE RASPBERRY PI 3 BOOTS
 * -----------------------------
 * The boot sequence on a Raspberry Pi is very different from a PC:
 *
 *  1. Power-on: the ARM cores are *held in reset*.  The VideoCore IV
 *     GPU starts executing from an on-chip ROM bootloader.
 *
 *  2. First-stage loader (ROM): the GPU reads start.elf and bootcode.bin
 *     from the FAT32 partition on the microSD card.
 *
 *  3. Second-stage loader (start.elf): the GPU reads config.txt and
 *     kernel8.img (for 64-bit / AArch64 mode) from the SD card and
 *     places it in RAM at physical address 0x80000.
 *
 *  4. ARM release: the GPU releases the ARM reset line.  Core 0 starts
 *     executing from address 0x80000 in EL2 (hypervisor exception level)
 *     or EL1 depending on config.txt.  Cores 1–3 spin in a tight loop
 *     waiting for a start address to be written to their mailbox registers.
 *
 *  5. This file: _start is placed at exactly 0x80000 by the linker script.
 *     We park the secondary cores, set up a stack, zero the BSS section,
 *     then call kernel_main().
 *
 * MULTI-CORE: WHY WE PARK CORES 1–3
 * -----------------------------------
 * All four Cortex-A53 cores begin execution at the same address (0x80000).
 * The MPIDR_EL1 (Multiprocessor Affinity Register) tells each core its own
 * ID in bits [1:0].  Cores 1–3 must be put to sleep (WFE — Wait For Event)
 * so only core 0 initialises hardware and enters the kernel.
 *
 * MEMORY MAP (simplified, Pi 3B)
 * --------------------------------
 *  0x00000000 – 0x0000FFFF   GPU / VC firmware reserved
 *  0x00080000                 Kernel load address (this code)
 *  0x3F000000 – 0x3FFFFFFF   BCM2837 peripheral MMIO (UART, GPIO, …)
 *  0x40000000 – 0x3FFFFFFFFF  ARM local peripherals (timer, mailboxes)
 */

.section ".text.boot"   /* placed first by the linker script */
.global _start

_start:
    /*
     * Read MPIDR_EL1 to determine which core we are running on.
     * Bits [1:0] hold the CPU ID: 0 = core 0, 1 = core 1, etc.
     * Only core 0 should proceed; all others enter a low-power wait loop.
     */
    mrs     x1, mpidr_el1
    and     x1, x1, #3         /* keep only the two low bits (core ID) */
    cbz     x1, .core0         /* if core ID == 0, jump to core0 init  */

.hang:
    /* Secondary cores park here.  WFE puts the core in a low-power state
     * until a WFE wake-up event is signalled (e.g. SEV from core 0). */
    wfe
    b       .hang

.core0:
    /*
     * Set up the stack.
     * The stack pointer must be valid before any C function is called.
     * We place the stack just below the kernel load address (0x80000),
     * growing downward into the memory the GPU left unused.
     * The linker exports _start as the base address of our kernel.
     */
    ldr     x1, =_start
    mov     sp, x1

    /*
     * Zero the BSS section.
     * The C standard guarantees that all global/static variables with no
     * explicit initialiser start at zero.  The ELF loader does NOT do this
     * for us on bare metal, so we must do it ourselves.
     * __bss_start and __bss_end are symbols defined by the linker script.
     */
    ldr     x1, =__bss_start
    ldr     x2, =__bss_end
.zero_bss:
    cmp     x1, x2
    b.ge    .call_main          /* stop when we reach the end of BSS */
    str     xzr, [x1], #8      /* store 0, then advance x1 by 8 bytes */
    b       .zero_bss

.call_main:
    /* Jump to the C kernel.  BL saves the return address in LR, but
     * kernel_main() should never return. */
    bl      kernel_main

    /* If kernel_main() returns (it shouldn't), halt all cores. */
.halt:
    wfe
    b       .halt
