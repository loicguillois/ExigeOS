/*
 * keyboard.c — PS/2 keyboard driver (x86)
 *
 * HOW THE PS/2 KEYBOARD CONTROLLER WORKS
 * ----------------------------------------
 * The PS/2 keyboard controller (Intel 8042 or compatible) mediates between
 * the keyboard and the CPU via two I/O ports:
 *
 *   Port 0x60 — Data port: read a scan code / write a command byte.
 *   Port 0x64 — Status/command port:
 *               read  → status register (bit 0 = output buffer full)
 *               write → send command to the controller itself
 *
 * SCAN CODES (Set 1)
 * -------------------
 * When a key is pressed, the keyboard sends one or more "make code" bytes.
 * When it is released, it sends "break codes" (make code | 0x80).
 * Scan Code Set 1 is the legacy set left active by most PC BIOSes:
 *   - Make  codes: 0x01–0x58 (bit 7 = 0)
 *   - Break codes: 0x81–0xD8 (bit 7 = 1)
 *
 * We discard break codes (bit 7 set) so we only react to key presses.
 * This means we cannot track modifier keys (Shift, Ctrl, Alt) — a
 * known limitation of this minimal driver.
 *
 * POLLING vs INTERRUPTS
 * ----------------------
 * A production kernel uses IRQ 1 (the PS/2 interrupt line) to receive
 * keystrokes asynchronously.  Here we poll: we spin in keyboard_getchar()
 * checking port 0x64 bit 0 (Output Buffer Full) until data arrives.
 * Simple, but wastes CPU cycles while waiting.
 *
 * AZERTY LAYOUT
 * --------------
 * Scan codes are fixed by the keyboard hardware; the OS layout is a
 * software translation layer.  Our table maps Set 1 codes to ASCII
 * following the French AZERTY layout (A↔Q, Z↔W, M at ';' position …).
 */

#include "keyboard.h"
#include "vga.h"
#include "io.h"
#include <stdint.h>

#define KB_DATA   0x60   /* PS/2 data port   */
#define KB_STATUS 0x64   /* PS/2 status port */

/*
 * Scan code → ASCII table (Set 1, AZERTY).
 * Index = 7-bit scan code.  Value = ASCII char or 0 for unmapped keys.
 *
 * Key AZERTY differences from QWERTY (scan code → AZERTY char):
 *   0x10 → 'a'   0x11 → 'z'   0x1E → 'q'   0x2C → 'w'   0x27 → 'm'
 */
static const char sc_azerty[128] = {
/*00*/  0,    0,   '1', '2', '3', '4', '5', '6',
/*08*/  '7',  '8', '9', '0', '-', '=', '\b', '\t',
/*10*/  'a',  'z', 'e', 'r', 't', 'y', 'u', 'i',
/*18*/  'o',  'p', '[', ']', '\n', 0,  'q', 's',
/*20*/  'd',  'f', 'g', 'h', 'j', 'k', 'l', 'm',
/*28*/  '\'', '`',  0,  '\\','w', 'x', 'c', 'v',
/*30*/  'b',  'n', ',', '.', '/', 0,   '*',  0,
/*38*/  0,   ' ',   0,   0,   0,   0,   0,   0,
/* 0x40+: function keys, arrow keys, numpad — not handled */
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
};

void keyboard_init(void) {
    /* Drain any stale bytes sitting in the PS/2 FIFO. */
    while (inb(KB_STATUS) & 0x01)
        inb(KB_DATA);
}

char keyboard_getchar(void) {
    uint8_t sc;
    for (;;) {
        while (!(inb(KB_STATUS) & 0x01));   /* poll: wait for data ready */
        sc = inb(KB_DATA);
        if (sc & 0x80) continue;    /* break code (key release) — skip */
        if (sc >= 128)  continue;   /* out-of-range — skip             */
        char c = sc_azerty[sc];
        if (c) return c;
    }
}

int keyboard_readline(char *buf, int max) {
    int len = 0;
    for (;;) {
        char c = keyboard_getchar();
        if (c == '\n') {
            buf[len] = '\0';
            vga_putchar('\n');
            return len;
        } else if (c == '\b') {
            if (len > 0) {
                len--;
                vga_putchar('\b');
            }
        } else if (len < max - 1) {
            buf[len++] = c;
            vga_putchar(c);
        }
    }
}
