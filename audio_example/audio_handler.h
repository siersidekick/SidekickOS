#ifndef AUDIO_HANDLER_H
#define AUDIO_HANDLER_H

#include <stddef.h> // For size_t
#include <stdint.h> // For uint8_t, uint16_t

// Buffers used by audio processing - can be extern if needed by other modules,
// or kept static within audio_handler.cpp if not. For now, keep them internal.
extern uint8_t *s_i2s_recording_buffer; 
extern uint8_t *s_audio_packet_buffer;

// External declaration for global audio frame count
extern uint16_t g_audio_frame_count;

void configure_microphone();
size_t read_microphone_data(uint8_t *buffer, size_t buffer_size); // More generic read
void deinit_microphone(); // Add deinit function
bool is_microphone_initialized();

#endif // AUDIO_HANDLER_H