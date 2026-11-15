/* MIT; see LICENSE. Protocol facts: PROTOCOL.md. */
#include "BluetoothHaptics.h"
#include <math.h>
#include <string.h>
#include <zlib.h>

uint32_t btChecksum(const uint8_t *packet, size_t bytes) {
    const uint8_t direction = 0xa2;
    uLong crc = crc32(0, &direction, 1);
    return (uint32_t)crc32(crc, packet, (uInt)bytes);
}
static void finishPacket(uint8_t *packet, size_t length) {
    uint32_t checksum = btChecksum(packet, length - 4);
    for (unsigned i = 0; i != 4; ++i) packet[length - 4 + i] = (uint8_t)(checksum >> (i * 8));
}
void btEncodeControl(uint8_t packet[78], uint8_t sequence, bool enable) {
    memset(packet, 0, 78);
    packet[0] = 0x31; packet[1] = (sequence & 15) * 16; packet[2] = 0x10;
    if (enable) {
        packet[3] = 0xb0; /* headphone/speaker gain + audio control */
        packet[4] = 0xc0; /* second audio control + actuator power */
        packet[7] = 0x7f; packet[8] = 0xff;
        /* audio route = 0, power attenuation = 0, classic rumble = 0 */
    } else {
        packet[3] = 0x03; /* restore ordinary rumble mode, with motors stopped */
    }
    finishPacket(packet, 78);
}
size_t btEncodePcm(uint8_t *packet, bool first, uint8_t sequence, uint8_t counter, const int8_t samples[128]) {
    const size_t length = first ? 206 : 142, offset = first ? 13 : 9;
    memset(packet, 0, length);
    packet[0] = first ? 0x33 : 0x32; packet[1] = (sequence & 15) * 16; packet[2] = 0x91;
    if (first) {
        packet[3] = 0x07; packet[4] = 0xfe;
        memset(packet + 5, 48, 5);
        packet[10] = counter; packet[11] = 0xd2; packet[12] = 64;
    } else {
        packet[3] = 0x03; packet[4] = 0x62; packet[5] = 48;
        packet[6] = counter; packet[7] = 0xd2; packet[8] = 64;
    }
    memcpy(packet + offset, samples, 128);
    finishPacket(packet, length);
    return length;
}
void btFilterInit(BtResampler *filter) {
    memset(filter, 0, sizeof(*filter));
    /* 48 kHz -> 3 kHz decimation: Blackman-windowed sinc, 1 kHz cutoff.
     * 193 taps add 2 ms group delay; suppress frequencies above the 1.5 kHz Nyquist limit. */
    double sum = 0;
    for (unsigned i = 0; i < BT_FILTER_TAPS; ++i) {
        double distance = (double)i - (BT_FILTER_TAPS - 1) / 2.0;
        double x = 2.0 * 1000.0 / 48000.0;
        double sinc = distance == 0 ? x : sin(M_PI * x * distance) / (M_PI * distance);
        double phase = 2 * M_PI * i / (BT_FILTER_TAPS - 1);
        filter->taps[i] = (float)(sinc * (0.42 - 0.5*cos(phase) + 0.08*cos(2*phase)));
        sum += filter->taps[i];
    }
    for (unsigned i = 0; i < BT_FILTER_TAPS; ++i) filter->taps[i] /= (float)sum;
}
size_t btResample(BtResampler *filter, const float *input, size_t frames, int8_t *output) {
    size_t produced = 0;
    for (size_t frame = 0; frame < frames; ++frame) {
        for (unsigned channel = 0; channel < 2; ++channel) {
            float value = input[2*frame+channel];
            filter->history[filter->cursor][channel] = isfinite(value) ? value : 0;
        }
        filter->cursor = (filter->cursor + 1) % BT_FILTER_TAPS;
        if (++filter->phase != 16) continue;
        filter->phase = 0;
        double sum[2] = { 0, 0 };
        unsigned index = filter->cursor;
        for (unsigned tap = 0; tap < BT_FILTER_TAPS; ++tap) {
            index = index == 0 ? BT_FILTER_TAPS - 1 : index - 1;
            sum[0] += filter->taps[tap] * filter->history[index][0];
            sum[1] += filter->taps[tap] * filter->history[index][1];
        }
        for (unsigned channel = 0; channel < 2; ++channel)
            output[2*produced+channel] = (int8_t)lrint(fmax(-1, fmin(1, sum[channel])) * 127);
        ++produced;
    }
    return produced;
}
