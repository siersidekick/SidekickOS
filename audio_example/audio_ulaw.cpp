#include "config.h"
#include "audio_ulaw.h"
#include "audio_handler.h"
#include <Arduino.h>
#include <stdint.h>
#include <string.h>

// μ-law encode a single 16-bit PCM sample
static uint8_t linear_to_ulaw(int16_t pcm_val) {
    const uint16_t BIAS = 0x84;
    const uint16_t CLIP = 32635;
    uint8_t sign = (pcm_val < 0) ? 0x80 : 0;
    if (pcm_val < 0) pcm_val = -pcm_val;
    if (pcm_val > CLIP) pcm_val = CLIP;
    pcm_val += BIAS;
    int exponent = 7;
    for (int expMask = 0x4000; (pcm_val & expMask) == 0 && exponent > 0; expMask >>= 1) {
        exponent--;
    }
    int mantissa = (pcm_val >> ((exponent == 0) ? 4 : (exponent + 3))) & 0x0F;
    uint8_t ulaw = ~(sign | (exponent << 4) | mantissa);
    return ulaw;
}

void process_and_send_ulaw_audio(BLECharacteristic *audio_characteristic) {
    extern uint8_t *s_i2s_recording_buffer;
    const size_t pcm_samples = FRAME_SIZE; // e.g. 160 for 8kHz, 20ms
    uint8_t ulaw_buffer[pcm_samples];
    size_t bytes_recorded = read_microphone_data(s_i2s_recording_buffer, pcm_samples * 2); // 16-bit PCM
    if (bytes_recorded < pcm_samples * 2) return;
    int16_t *pcm = (int16_t *)s_i2s_recording_buffer;
    for (size_t i = 0; i < pcm_samples; ++i) {
        ulaw_buffer[i] = linear_to_ulaw(pcm[i]);
    }
    if (audio_characteristic) {
        audio_characteristic->setValue(ulaw_buffer, pcm_samples);
        audio_characteristic->notify();
    }
}
