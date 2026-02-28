/*
 * sound_stub.c — No-op audio stubs for Raspberry Pi 3B
 *
 * The PC speaker and the Intel 8253/8254 PIT do not exist on the
 * Raspberry Pi.  These stub functions satisfy the sound.h interface
 * so that shell.c compiles unchanged on both platforms.
 *
 * Future work: the BCM2837 has a PWM (Pulse Width Modulation) controller
 * that could drive a piezo buzzer connected to GPIO 18 (PWM channel 0).
 * A full implementation would:
 *   1. Configure the PWM clock via the Clock Manager (CM_PWMCTL).
 *   2. Set the PWM range to achieve the desired frequency.
 *   3. Enable the PWM output on GPIO 18 (ALT5 function).
 *   4. Use the system timer for precise note durations.
 */

#include "sound.h"
#include <stdint.h>

void sound_play(uint32_t freq_hz, uint32_t duration_ms) {
    (void)freq_hz;
    (void)duration_ms;
}

void sound_stop(void) {}

void sound_note(const char *name) {
    (void)name;
}

void sound_play_sequence(const char *str) {
    (void)str;
}
