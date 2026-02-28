/*
 * sound.h — PC speaker / audio driver interface
 *
 * On x86, sound is produced through the PC speaker, driven by
 * channel 2 of the Intel 8253/8254 Programmable Interval Timer (PIT).
 *
 * On Raspberry Pi 3, there is no PC speaker and the functions are
 * compiled as no-ops (see sound_stub.c).
 */

#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

/* sound_play() — Play a tone at freq_hz for duration_ms milliseconds.
 *   freq_hz    : frequency in Hz (0 = silence / rest)
 *   duration_ms: duration in milliseconds
 * The function blocks until the tone is finished. */
void sound_play(uint32_t freq_hz, uint32_t duration_ms);

/* sound_stop() — Immediately silence the PC speaker. */
void sound_stop(void);

/* sound_note() — Play a single named note for a fixed duration.
 *   name: one of "do" "re" "mi" "fa" "sol" "la" "si"
 * Unknown names are silently ignored. */
void sound_note(const char *name);

/* sound_play_sequence() — Play multiple space-separated note names.
 *   str: e.g. "sol sol sol mi si sol mi si sol"
 * A short silence is inserted between each note for articulation. */
void sound_play_sequence(const char *str);

#endif
