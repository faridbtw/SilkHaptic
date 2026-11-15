/* Independent Bluetooth packet encoder / resampler. MIT; see LICENSE.
 * Wire-layout references are recorded in PROTOCOL.md. */
#ifndef SILKSOFT_BLUETOOTH_H
#define SILKSOFT_BLUETOOTH_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define BT_HAPTIC_FRAMES 64
#define BT_INPUT_FRAMES 1024
#define BT_FILTER_TAPS 193
typedef struct {
    float taps[BT_FILTER_TAPS];
    float history[BT_FILTER_TAPS][2];
    unsigned cursor, phase;
} BtResampler;
void btFilterInit(BtResampler *filter);
size_t btResample(BtResampler *filter, const float *input, size_t frames, int8_t *output);
size_t btEncodePcm(uint8_t *packet, bool first, uint8_t sequence, uint8_t counter, const int8_t samples[128]);
void btEncodeControl(uint8_t packet[78], uint8_t sequence, bool enable);
uint32_t btChecksum(const uint8_t *packet, size_t bytes);
#endif
