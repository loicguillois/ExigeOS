/*
 * keyboard.h — Keyboard driver interface
 *
 * On x86 the driver reads PS/2 scan codes from port 0x60.
 * On Raspberry Pi 3 the driver reads bytes from the UART (serial
 * terminal), since there is no PS/2 controller on the board.
 *
 * Both implementations expose the same three functions so that
 * shell.c and kernel.c compile unchanged on either platform.
 */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

/* keyboard_init() — Initialise the keyboard hardware.
 *   x86 : flushes any stale bytes in the PS/2 FIFO.
 *   RPi3: UART already initialised by vga_init(); this is a no-op. */
void keyboard_init(void);

/* keyboard_getchar() — Blocking read of one character.
 * Spins until a printable keystroke (or Enter / Backspace) is
 * available, then returns its ASCII value. */
char keyboard_getchar(void);

/* keyboard_readline() — Read a line of text into buf.
 *   buf : destination buffer (null-terminated on return)
 *   max : size of buf in bytes (including the null terminator)
 * Echoes typed characters to the screen and handles backspace.
 * Returns the number of characters written (not counting '\0'). */
int keyboard_readline(char *buf, int max);

#endif
