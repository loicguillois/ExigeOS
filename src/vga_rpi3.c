/*
 * vga_rpi3.c — Display driver for Raspberry Pi 3B via UART PL011
 *
 * WHY UART INSTEAD OF VGA?
 * -------------------------
 * The Raspberry Pi has no VGA connector and no 0xB8000 text buffer.
 * Instead, we use the PL011 UART (Universal Asynchronous
 * Receiver/Transmitter) mapped to a serial terminal.
 *
 * In QEMU, "-serial stdio" connects UART0 to your host terminal.
 * On real hardware, you would connect a USB-to-serial adapter to
 * GPIO pins 14 (TXD) and 15 (RXD).
 *
 * This file implements the same interface as vga.h so that shell.c
 * and kernel.c compile without modification on both platforms.
 * ANSI escape codes replace VGA attribute bytes for colour support.
 *
 * PL011 UART (UART0) — BCM2837 (Pi 3B)
 * ----------------------------------------
 * The BCM2837 peripheral base address is 0x3F000000.
 * PL011 UART0 starts at offset 0x201000, so base = 0x3F201000.
 *
 * Key registers (offsets from 0x3F201000):
 *   0x00  DR    — Data Register: write a byte to TX, read from RX
 *   0x18  FR    — Flag Register: bits indicate FIFO state
 *                   bit 5 (TXFF): TX FIFO full  → wait before writing
 *                   bit 4 (RXFE): RX FIFO empty → wait before reading
 *   0x24  IBRD  — Integer part of baud rate divisor
 *   0x28  FBRD  — Fractional part of baud rate divisor
 *   0x2C  LCRH  — Line Control (data bits, parity, stop bits, FIFO enable)
 *   0x30  CR    — Control Register (enable UART, TX, RX)
 *   0x44  ICR   — Interrupt Clear Register
 *
 * BAUD RATE CALCULATION
 * ----------------------
 * The UART clock on BCM2837 is 48 MHz.
 * Divisor = UART_CLK / (16 × baud_rate) = 48 000 000 / (16 × 115 200) = 26.04
 *   IBRD = 26   (integer part)
 *   FBRD = round(0.04 × 64) = 3   (fractional part × 64)
 *
 * MEMORY-MAPPED I/O (MMIO) ON ARM
 * ---------------------------------
 * Unlike x86, ARM has no separate I/O port space.  All peripheral
 * registers are accessed via regular load/store instructions to
 * specific physical addresses.  We use volatile uint32_t * pointers
 * to prevent the compiler from optimising away the accesses.
 */

#include "vga.h"
#include <stdint.h>

#define UART_BASE  ((volatile uint32_t *)0x3F201000UL)

#define UART_DR    (UART_BASE + 0x00/4)   /* Data Register       */
#define UART_FR    (UART_BASE + 0x18/4)   /* Flag Register       */
#define UART_IBRD  (UART_BASE + 0x24/4)   /* Integer baud rate   */
#define UART_FBRD  (UART_BASE + 0x28/4)   /* Fractional baud rate*/
#define UART_LCRH  (UART_BASE + 0x2C/4)   /* Line control        */
#define UART_CR    (UART_BASE + 0x30/4)   /* Control             */
#define UART_ICR   (UART_BASE + 0x44/4)   /* Interrupt clear     */

#define FR_TXFF  (1 << 5)   /* TX FIFO full  — spin before writing */
#define FR_RXFE  (1 << 4)   /* RX FIFO empty — spin before reading */

/* ── UART initialisation ──────────────────────────────────────────────────── */

void vga_init(void) {
    /* Step 1: disable the UART before changing any settings. */
    *UART_CR = 0;

    /* Step 2: clear all pending interrupts. */
    *UART_ICR = 0x7FF;

    /* Step 3: set baud rate to 115200 bps (clock = 48 MHz).
     *   IBRD = 26, FBRD = 3  (see calculation above) */
    *UART_IBRD = 26;
    *UART_FBRD = 3;

    /* Step 4: configure the line:
     *   bits [6:5] = 11 → 8-bit data
     *   bit  [4]   =  1 → enable TX/RX FIFOs
     *   other bits  =  0 → 1 stop bit, no parity */
    *UART_LCRH = (3 << 5) | (1 << 4);

    /* Step 5: enable the UART, TX path, and RX path. */
    *UART_CR = (1 << 0) | (1 << 8) | (1 << 9);

    vga_clear();
}

/* ── Low-level send ─────────────────────────────────────────────────────── */

static void uart_putchar(char c) {
    /* Spin until the TX FIFO has room for one byte. */
    while (*UART_FR & FR_TXFF);
    *UART_DR = (uint32_t)(uint8_t)c;
}

static void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') uart_putchar('\r');  /* CRLF translation */
        uart_putchar(*s++);
    }
}

/* ── vga.h interface implemented over UART ─────────────────────────────── */

/* ANSI escape: erase screen and move cursor to top-left. */
void vga_clear(void) {
    uart_puts("\033[2J\033[H");
}

void vga_putchar(char c) {
    if (c == '\n') uart_putchar('\r');
    uart_putchar(c);
}

void vga_print(const char *str) {
    uart_puts(str);
}

void vga_newline(void) {
    uart_puts("\r\n");
}

void vga_print_int(uint32_t n) {
    if (n == 0) { uart_putchar('0'); return; }
    char buf[12];
    int i = 0;
    while (n > 0) { buf[i++] = '0' + (int)(n % 10); n /= 10; }
    for (int j = i - 1; j >= 0; j--)
        uart_putchar(buf[j]);
}

void vga_print_int2(uint8_t n) {
    uart_putchar('0' + n / 10);
    uart_putchar('0' + n % 10);
}

/*
 * ANSI foreground colour codes (standard 8 + bright 8).
 * Indexed by the vga_color_t value so the same colour names work on RPi.
 */
static const char *ansi_fg[16] = {
    "\033[30m",  /* black        */
    "\033[34m",  /* blue         */
    "\033[32m",  /* green        */
    "\033[36m",  /* cyan         */
    "\033[31m",  /* red          */
    "\033[35m",  /* magenta      */
    "\033[33m",  /* brown/yellow */
    "\033[37m",  /* light grey   */
    "\033[90m",  /* dark grey    */
    "\033[94m",  /* light blue   */
    "\033[92m",  /* light green  */
    "\033[96m",  /* light cyan   */
    "\033[91m",  /* light red    */
    "\033[95m",  /* light magenta*/
    "\033[93m",  /* yellow       */
    "\033[97m",  /* white        */
};

void vga_set_color(uint8_t fg, uint8_t bg) {
    (void)bg;   /* background colour not supported over UART */
    if (fg < 16) uart_puts(ansi_fg[fg]);
}

/* ANSI escape ?5h/l toggles reverse-video mode for the visual bell. */
void vga_flash(void) {
    uart_puts("\033[?5h");                      /* reverse video ON  */
    for (volatile int i = 0; i < 2000000; i++);
    uart_puts("\033[?5l");                      /* reverse video OFF */
}
