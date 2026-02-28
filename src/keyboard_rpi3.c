/*
 * keyboard_rpi3.c — Input driver for Raspberry Pi 3B via PL011 UART
 *
 * The Raspberry Pi has no PS/2 controller.  Input comes from the same
 * PL011 UART used for output (vga_rpi3.c).
 *
 * Under QEMU with "-serial stdio": characters typed in the host terminal
 * appear in the UART RX FIFO as plain bytes.
 * On real hardware: connect a USB-to-serial adapter to GPIO 14/15 and
 * use a terminal emulator (minicom, screen) at 115200 8N1.
 *
 * READING FROM THE UART
 * ----------------------
 * FR bit 4 (RXFE = RX FIFO Empty): poll this until it is clear, then
 * read the received byte from DR.
 *
 * BACKSPACE HANDLING ON A SERIAL TERMINAL
 * -----------------------------------------
 * Different host terminals send different codes for Backspace:
 *   0x08 (BS)  — older VT100-style terminals
 *   0x7F (DEL) — xterm, GNOME Terminal, most modern emulators
 * We accept both.  To visually erase the character on screen we send
 * the three-byte sequence: BS + SPACE + BS.
 */

#include "keyboard.h"
#include "vga.h"
#include <stdint.h>

#define UART_BASE ((volatile uint32_t *)0x3F201000UL)
#define UART_DR   (UART_BASE + 0x00/4)
#define UART_FR   (UART_BASE + 0x18/4)
#define FR_RXFE   (1 << 4)

/* UART is already initialised by vga_init() — nothing to do here. */
void keyboard_init(void) {}

char keyboard_getchar(void) {
    while (*UART_FR & FR_RXFE);     /* spin until a byte arrives */
    return (char)(*UART_DR & 0xFF);
}

int keyboard_readline(char *buf, int max) {
    int len = 0;
    for (;;) {
        char c = keyboard_getchar();
        if (c == '\r' || c == '\n') {
            buf[len] = '\0';
            vga_putchar('\n');
            return len;
        } else if (c == '\b' || c == 127) {     /* BS or DEL */
            if (len > 0) {
                len--;
                vga_putchar('\b');
                vga_putchar(' ');
                vga_putchar('\b');
            }
        } else if (c >= 32 && len < max - 1) {
            buf[len++] = c;
            vga_putchar(c);
        }
    }
}
