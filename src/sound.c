/*
 * sound.c — PC speaker driver (x86)
 *
 * HOW THE PC SPEAKER WORKS
 * -------------------------
 * The PC speaker is connected to the output of PIT (Programmable Interval
 * Timer) channel 2.  To play a tone, we:
 *   1. Program PIT channel 2 to produce a square wave at the desired
 *      frequency by writing a divisor to port 0x42.
 *   2. Enable the speaker output by setting bits 0 and 1 of I/O port 0x61
 *      (the System Control Port B / PC speaker gate).
 *
 * To stop the sound, we clear bits 0-1 of port 0x61 to disconnect the
 * PIT output from the speaker.
 *
 * THE 8253/8254 PIT — THREE INDEPENDENT CHANNELS
 * ------------------------------------------------
 * The Intel 8253/8254 Programmable Interval Timer has three 16-bit
 * countdown channels, all clocked at 1,193,180 Hz (≈ 1.19 MHz):
 *
 *   Channel 0 (port 0x40): System timer.
 *     Initialised by the BIOS in Mode 3 (square wave) with divisor 0
 *     (= 65536), giving a frequency of 1193180/65536 ≈ 18.2 Hz.
 *     The OS uses IRQ0 (connected to this channel) for time-keeping.
 *     We use it here to measure real elapsed time without needing IRQs.
 *
 *   Channel 1 (port 0x41): Historically used for DRAM refresh.
 *     Obsolete on modern hardware; we ignore it.
 *
 *   Channel 2 (port 0x42): PC speaker.
 *     We program this channel to produce a square wave at a specific
 *     frequency.  Divisor = 1,193,180 / desired_frequency_Hz.
 *
 * CONTROL REGISTER (port 0x43)
 * -----------------------------
 * Writing a "control word" to port 0x43 selects the channel and mode.
 * For channel 2, Mode 3 (square wave generator), 16-bit access:
 *   0xB6 = 1011 0110
 *          ^^^^ ──── channel 2 (bits 7-6 = 10)
 *               ^^ ─ read/write low byte then high byte (bits 5-4 = 11)
 *                 ^^^ mode 3 square wave (bits 3-1 = 011)
 *                    ^ BCD=0, binary counting (bit 0 = 0)
 *
 * CHANNEL 0 LATCH READ (for timing)
 * -----------------------------------
 * Sending 0x00 to port 0x43 latches the current counter value of
 * channel 0 into a holding register.  Two subsequent reads from port
 * 0x40 give the low byte then the high byte of the latched count.
 * This lets us sample the countdown without disturbing it.
 *
 * PORT 0x61 — SPEAKER GATE
 * --------------------------
 *   bit 0: enable PIT channel 2 → speaker connection
 *   bit 1: enable speaker output
 * Both bits must be set to make the speaker produce sound.
 *
 * TIMING: WHY NOT USE A BUSY-WAIT LOOP?
 * ---------------------------------------
 * A naive busy-wait (for(volatile i=0; i<N; i++)) runs at the actual CPU
 * execution speed, which QEMU does not emulate at real time.  Loops that
 * would take 1 second on real hardware complete in microseconds in QEMU.
 * We instead read PIT channel 0's counter directly to measure real elapsed
 * time, independent of CPU clock speed.
 *
 * MUSICAL NOTES (equal temperament, 4th octave)
 * -----------------------------------------------
 * Equal temperament divides one octave (2× frequency) into 12 equal
 * semitones.  The reference pitch is A4 = 440 Hz (international standard
 * ISO 16).  The solfège names correspond to:
 *   do=C4=262 Hz,  re=D4=294 Hz,  mi=E4=330 Hz,  fa=F4=349 Hz,
 *   sol=G4=392 Hz, la=A4=440 Hz,  si=B4=494 Hz
 */

#include "sound.h"
#include "io.h"
#include <stdint.h>

#define PIT_BASE_FREQ 1193180UL   /* PIT input clock frequency in Hz */

/* ── Note table ─────────────────────────────────────────────────── */

typedef struct {
    const char *name;
    uint32_t    freq;
} note_t;

static const note_t notes[] = {
    { "do",  262 },
    { "re",  294 },
    { "mi",  330 },
    { "fa",  349 },
    { "sol", 392 },
    { "la",  440 },
    { "si",  494 },
    { 0, 0 }
};

/* Simple string equality helper (no libc available). */
static int str_eq_sound(const char *a, const char *b) {
    while (*a && *b)
        if (*a++ != *b++) return 0;
    return *a == *b;
}

/* ── Real-time delay via PIT channel 0 ──────────────────────────── */

/*
 * pit0_read() — Latch and return the current channel 0 counter value.
 *
 * Protocol:
 *   1. Write 0x00 to port 0x43 → send latch command to channel 0.
 *   2. Read port 0x40 twice → low byte, then high byte.
 *
 * The counter decrements from 65535 to 0 at 1,193,180 Hz, then wraps.
 * One decrement ≈ 0.84 µs; 1 ms ≈ 1193 ticks.
 */
static uint16_t pit0_read(void) {
    outb(0x43, 0x00);               /* latch channel 0 counter value */
    uint8_t lo = inb(0x40);
    uint8_t hi = inb(0x40);
    return (uint16_t)((uint16_t)hi << 8 | lo);
}

/*
 * delay_ms() — Spin for at least `ms` real milliseconds.
 *
 * We repeatedly latch the PIT channel 0 counter and accumulate the
 * difference between successive readings.  Because the counter counts
 * DOWN and wraps around, we handle the wrap explicitly:
 *   - If prev >= curr, the counter simply counted down: delta = prev - curr.
 *   - If prev < curr, a wrap occurred: delta = prev + (65536 - curr).
 *
 * 1 ms ≈ 1193180 / 1000 = 1193 ticks.
 */
static void delay_ms(uint32_t ms) {
    uint32_t ticks_needed = ms * 1193UL;
    uint16_t prev    = pit0_read();
    uint32_t elapsed = 0;

    while (elapsed < ticks_needed) {
        uint16_t curr  = pit0_read();
        uint16_t delta = (prev >= curr)
                         ? (prev - curr)
                         : (uint16_t)(prev + (65536U - curr));
        elapsed += delta;
        prev = curr;
    }
}

/* ── PC speaker driver ───────────────────────────────────────────── */

/*
 * pit_set_divisor() — Program PIT channel 2 for a given frequency.
 *
 * Control word 0xB6: channel 2, Mode 3 (square wave), 16-bit load.
 * Divisor: the counter is loaded with PIT_BASE_FREQ / freq_hz.
 * We write the low byte first, then the high byte (as required by the
 * 16-bit access mode specified in the control word).
 */
static void pit_set_divisor(uint16_t divisor) {
    outb(0x43, 0xB6);                        /* channel 2, Mode 3 */
    outb(0x42, (uint8_t)(divisor & 0xFF));   /* low byte  */
    outb(0x42, (uint8_t)(divisor >> 8));     /* high byte */
}

/*
 * sound_play() — Play a tone at freq_hz for duration_ms milliseconds.
 *
 * If freq_hz is 0, we produce silence (useful for rests between notes).
 * Port 0x61 bits 0-1 gate the PIT channel 2 output to the speaker.
 * We read the current value first to preserve bits 2-7 (other hardware).
 */
void sound_play(uint32_t freq_hz, uint32_t duration_ms) {
    if (freq_hz == 0) {
        sound_stop();
        delay_ms(duration_ms);
        return;
    }

    uint16_t divisor = (uint16_t)(PIT_BASE_FREQ / freq_hz);
    pit_set_divisor(divisor);

    /* Enable PIT channel 2 gate (bit 0) and speaker output (bit 1). */
    uint8_t tmp = inb(0x61);
    outb(0x61, tmp | 0x03);

    delay_ms(duration_ms);

    sound_stop();
}

/*
 * sound_stop() — Disconnect the speaker from the PIT.
 *
 * Clear bits 0-1 of port 0x61.  The speaker goes silent immediately.
 */
void sound_stop(void) {
    uint8_t tmp = inb(0x61);
    outb(0x61, tmp & ~0x03);
}

/*
 * sound_note() — Look up a note by solfège name and play it for 450 ms.
 *
 * Unknown names are silently ignored.  450 ms gives a moderate note
 * length suitable for simple melodies at about 130 BPM.
 */
void sound_note(const char *name) {
    for (int i = 0; notes[i].name != 0; i++) {
        if (str_eq_sound(name, notes[i].name)) {
            sound_play(notes[i].freq, 450);
            return;
        }
    }
}

/*
 * sound_play_sequence() — Play a space-separated sequence of note names.
 *
 * Example: "do re mi fa sol la si"
 *
 * Tokens are split on ASCII space ' '.  Each token is looked up in the
 * note table and played at 450 ms.  An 80 ms silence is inserted between
 * notes to separate ("articulate") them, preventing them from blending
 * into a single sustained tone.
 *
 * Maximum token length is 7 characters (enough for "sol\0" with room
 * to spare); longer tokens are silently truncated.
 */
void sound_play_sequence(const char *str) {
    char token[8];
    int  ti = 0;

    while (1) {
        char c = *str;
        if (c == ' ' || c == '\0') {
            if (ti > 0) {
                token[ti] = '\0';
                sound_note(token);
                sound_play(0, 80);   /* articulation gap */
                ti = 0;
            }
            if (c == '\0') break;
        } else if (ti < 7) {
            token[ti++] = c;
        }
        str++;
    }
}
