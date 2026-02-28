/*
 * shell.c — Interactive command shell
 *
 * OVERVIEW
 * ---------
 * The shell is the highest-level component of ExigeOS.  After the kernel
 * initialises the display and keyboard, it calls shell_run(), which loops
 * forever:
 *   1. Print the prompt ("Kernel# ").
 *   2. Read a line of input from the keyboard.
 *   3. Parse the command name and optional argument.
 *   4. Dispatch to the appropriate handler function.
 *
 * COMMAND PARSING
 * ----------------
 * The input buffer holds at most BUF_SIZE characters.  split_arg() walks
 * the buffer to find the first space, null-terminates the command name in
 * place, and returns a pointer to the remainder (the argument string).
 * If there is no space, it returns NULL.
 *
 * Example:  buf = "note do re mi\0"
 *   After split_arg(): buf = "note\0", arg = "do re mi"
 *
 * PLATFORM GUARDS
 * ----------------
 * Some commands rely on x86-specific I/O ports (CMOS RTC, PS/2 reset).
 * They are compiled out on RPi3 using #ifndef PLATFORM_RPI3.
 *
 * CMOS REAL-TIME CLOCK (x86 only)
 * ---------------------------------
 * The CMOS chip contains a battery-backed real-time clock (RTC).  Its
 * registers are accessed via an index/data port pair:
 *
 *   Port 0x70 (write): select register index (0x00–0x3F).
 *   Port 0x71 (read):  read the selected register.
 *
 * io_wait() inserts a tiny delay between the index write and data read
 * to give the CMOS chip time to respond (important on fast CPUs).
 *
 * RTC register map (relevant subset):
 *   0x00 = seconds    0x02 = minutes    0x04 = hours
 *   0x07 = day        0x08 = month      0x09 = year (last two digits)
 *   0x32 = century
 *
 * Values are stored in BCD (Binary Coded Decimal): each nibble holds one
 * decimal digit.  bcd2dec() converts: (high_nibble × 10) + low_nibble.
 *
 * SYSTEM RESET
 * -------------
 * x86: Write 0xFE to port 0x64 (PS/2 controller command port).
 *   This pulses the CPU RESET# line, causing an immediate hard reset.
 *   Equivalent to pressing the physical reset button.
 *
 * RPi3: Write to the BCM2837 Power Management / Watchdog registers.
 *   PM_RSTC (0x3F10001C): reset control register.
 *   PM_WDOG (0x3F100024): watchdog timer (set to 32 ticks ≈ immediate).
 *   The magic password 0x5A000000 must be ORed into every write to
 *   protect against accidental resets.
 */

#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "sound.h"
#ifndef PLATFORM_RPI3
#  include "io.h"
#endif
#include <stdint.h>

/* ── Utilities ───────────────────────────────────────────────────── */

/* Case-sensitive string equality (no libc available). */
static int str_eq(const char *a, const char *b) {
    while (*a && *b)
        if (*a++ != *b++) return 0;
    return *a == *b;
}

/* Wrapper: print a two-digit integer (delegates to vga_print_int2). */
static void print_uint8_2digits(uint8_t n) {
    vga_print_int2(n);
}

/*
 * split_arg() — In-place command/argument splitter.
 *
 * Scans buf for the first space.  If found:
 *   - Replaces the space with '\0' (terminates the command token).
 *   - Returns a pointer to the character after the space (the argument).
 * If no space is found, returns NULL (command has no argument).
 */
static char *split_arg(char *buf) {
    char *p = buf;
    while (*p && *p != ' ') p++;
    if (*p == ' ') {
        *p = '\0';
        return p + 1;
    }
    return 0;
}

/* ── CMOS / RTC (x86 only) ───────────────────────────────────────── */

#ifndef PLATFORM_RPI3
/*
 * cmos_read() — Read one byte from the CMOS RTC.
 *
 * The CMOS chip requires: write index to 0x70, then read data from 0x71.
 * io_wait() (a few PORT 0x80 writes) creates a short delay to let the
 * slow CMOS chip settle before the data read.
 */
static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    io_wait();
    return inb(0x71);
}

/*
 * bcd2dec() — Convert a BCD byte to a binary integer.
 *
 * BCD packs two decimal digits into one byte:
 *   high nibble (bits 7-4) = tens digit
 *   low  nibble (bits 3-0) = units digit
 * Example: 0x47 (BCD) → 47 (decimal).
 */
static uint8_t bcd2dec(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}
#endif

/* ── Visual bell ─────────────────────────────────────────────────── */

/*
 * cmd_beep() — Flash the screen as a visual bell.
 *
 * Called by the "beep" command.  vga_flash() inverts all cell colours
 * for a short delay, then restores them — a classic visual alert when
 * an audio beep is unavailable or unwanted.
 */
static void cmd_beep(void) {
    vga_flash();
}

/* ── System reset ─────────────────────────────────────────────────── */

/*
 * cmd_reboot() — Perform a hard reset.
 *
 * x86: The PS/2 keyboard controller (Intel 8042) can assert the CPU's
 *   RESET# line by command 0xFE sent to its command port 0x64.  This is
 *   the standard software reboot mechanism on PC hardware.  The hlt loop
 *   is defensive; it should never execute after the reset fires.
 *
 * RPi3: BCM2837 provides a watchdog timer in the Power Management block.
 *   Setting PM_WDOG to a small tick count and PM_RSTC to the full-chip-
 *   reset mode triggers an immediate hardware reset.  Every write must
 *   include the password 0x5A000000 in the upper 24 bits, or the write
 *   is ignored by the hardware.
 */
static void cmd_reboot(void) {
#ifndef PLATFORM_RPI3
    outb(0x64, 0xFE);                       /* PS/2: pulse CPU RESET# line */
    for (;;) __asm__ volatile ("hlt");
#else
    volatile uint32_t *PM_WDOG = (volatile uint32_t *)0x3F100024UL;
    volatile uint32_t *PM_RSTC = (volatile uint32_t *)0x3F10001CUL;
    *PM_WDOG = 0x5A000020U;                 /* watchdog timeout: 32 ticks  */
    *PM_RSTC = 0x5A000020U;                 /* full chip reset             */
    for (;;) __asm__ volatile ("wfe");
#endif
}

/* ── Text colour ─────────────────────────────────────────────────── */

/*
 * VGA colour attribute table.
 * Maps English colour names to 4-bit VGA foreground colour codes.
 * The 16 VGA colours are defined by the attribute byte format:
 *   bits 3-0: foreground (0-15)  bits 6-4: background (0-7)  bit 7: blink
 */
typedef struct { const char *name; uint8_t code; } color_entry_t;

static const color_entry_t color_table[] = {
    { "black",        0  },
    { "blue",         1  },
    { "green",        2  },
    { "cyan",         3  },
    { "red",          4  },
    { "magenta",      5  },
    { "brown",        6  },
    { "grey",         7  },
    { "darkgrey",     8  },
    { "lightblue",    9  },
    { "lightgreen",   10 },
    { "lightcyan",    11 },
    { "lightred",     12 },
    { "lightmagenta", 13 },
    { "yellow",       14 },
    { "white",        15 },
    { 0, 0 }
};

static void cmd_color(const char *name) {
    if (!name) {
        vga_newline();
        vga_print("Usage: color <name>  (e.g. color white)");
        vga_newline();
        return;
    }
    for (int i = 0; color_table[i].name; i++) {
        if (str_eq(name, color_table[i].name)) {
            vga_set_color(color_table[i].code, 0);
            return;
        }
    }
    vga_newline();
    vga_print("Unknown color name.");
    vga_newline();
}

/* ── Built-in commands ───────────────────────────────────────────── */

static void cmd_cls(void) {
    vga_clear();
}

static void cmd_help(void) {
    vga_newline();
    vga_print("Available commands:"); vga_newline();
    vga_newline();
    vga_print("  reboot  : restart the computer");              vga_newline();
    vga_print("  cls     : clear the screen");                  vga_newline();
    vga_print("  beep    : visual flash (screen bell)");        vga_newline();
    vga_print("  note    : play notes (do re mi fa sol la si)"); vga_newline();
    vga_print("  color   : change text foreground color");      vga_newline();
    vga_print("  date    : display current date");              vga_newline();
    vga_print("  time    : display current time");              vga_newline();
    vga_print("  help    : list available commands");           vga_newline();
}

/*
 * cmd_date() — Read and display the current date from the CMOS RTC.
 *
 * Registers: day (0x07), month (0x08), year (0x09), century (0x32).
 * All values are BCD-encoded and must be decoded before display.
 * Output format: DD/MM/YYYY
 */
static void cmd_date(void) {
#ifndef PLATFORM_RPI3
    uint8_t day   = bcd2dec(cmos_read(0x07));
    uint8_t month = bcd2dec(cmos_read(0x08));
    uint8_t year  = bcd2dec(cmos_read(0x09));
    uint8_t cent  = bcd2dec(cmos_read(0x32));
    vga_newline();
    print_uint8_2digits(day);   vga_putchar('/');
    print_uint8_2digits(month); vga_putchar('/');
    vga_print_int2(cent);
    vga_print_int2(year);
    vga_newline();
#else
    vga_newline();
    vga_print("Not available on RPi3 (no RTC)");
    vga_newline();
#endif
}

/*
 * cmd_time() — Read and display the current time from the CMOS RTC.
 *
 * Registers: hours (0x04), minutes (0x02), seconds (0x00).
 * All values are BCD-encoded.  Output format: HH:MM:SS
 */
static void cmd_time(void) {
#ifndef PLATFORM_RPI3
    uint8_t h = bcd2dec(cmos_read(0x04));
    uint8_t m = bcd2dec(cmos_read(0x02));
    uint8_t s = bcd2dec(cmos_read(0x00));
    vga_newline();
    print_uint8_2digits(h); vga_putchar(':');
    print_uint8_2digits(m); vga_putchar(':');
    print_uint8_2digits(s);
    vga_newline();
#else
    vga_newline();
    vga_print("Not available on RPi3 (no RTC)");
    vga_newline();
#endif
}

/* ── Main shell loop ─────────────────────────────────────────────── */

#define BUF_SIZE 128   /* max characters per input line (including NUL) */

/*
 * shell_run() — Enter the interactive command loop (never returns).
 *
 * Each iteration:
 *   1. Print the prompt.
 *   2. keyboard_readline() blocks until the user presses Enter,
 *      then returns the typed line in buf (NUL-terminated, no newline).
 *   3. split_arg() separates the command from its argument in-place.
 *   4. str_eq() dispatches to the correct handler.
 */
void shell_run(void) {
    char buf[BUF_SIZE];

    for (;;) {
        vga_newline();
        vga_print("Kernel# ");
        keyboard_readline(buf, BUF_SIZE);

        char *arg = split_arg(buf);

        if      (str_eq(buf, "reboot")) { cmd_reboot(); }
        else if (str_eq(buf, "cls"))    { cmd_cls(); }
        else if (str_eq(buf, "help"))   { cmd_help(); }
        else if (str_eq(buf, "beep"))   { cmd_beep(); }
        else if (str_eq(buf, "note"))   { if (arg) sound_play_sequence(arg); }
        else if (str_eq(buf, "color"))  { cmd_color(arg); }
        else if (str_eq(buf, "date"))   { cmd_date(); }
        else if (str_eq(buf, "time"))   { cmd_time(); }
        else if (buf[0] != '\0') {
            vga_newline();
            vga_print("Unknown command. Type 'help' to list commands.");
            vga_newline();
        }
    }
}
