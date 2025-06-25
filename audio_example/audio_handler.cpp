#include "audio_handler.h"
#include "config.h"
#include "logger.h"
#include <Arduino.h>
#include "driver/i2s.h"

size_t i2s_recording_buffer_size = FRAME_SIZE * (SAMPLE_BITS / 8);
size_t audio_packet_buffer_size = (FRAME_SIZE * (SAMPLE_BITS / 8)) + AUDIO_FRAME_HEADER_LEN;

uint8_t *s_i2s_recording_buffer = nullptr;
uint8_t *s_audio_packet_buffer = nullptr;
uint16_t g_audio_frame_count = 0;

static bool i2s_driver_installed = false;

// I2S port and pin config for ESP32-S3 (update pins as needed)
#define I2S_PORT I2S_NUM_0
#define I2S_MIC_SERIAL_CLOCK 42
#define I2S_MIC_SERIAL_DATA 41

void configure_microphone()
{
    logger_printf("\n");
    // I2S config for PDM mic (mono, 8kHz, 16-bit)
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = FRAME_SIZE,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_MIC_SERIAL_CLOCK, // Serial clock (BCK)
        .ws_io_num = -1,                    // Not used for PDM
        .data_out_num = -1,                 // Not used (RX only)
        .data_in_num = I2S_MIC_SERIAL_DATA  // Serial data in
    };

    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        logger_printf("[MIC] ERROR: Failed to install I2S driver! Code: 0x%x\n", err);
        i2s_driver_installed = false;
        return;
    } else {
        i2s_driver_installed = true;
    }
    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        logger_printf("[MIC] ERROR: Failed to set I2S pins! Code: 0x%x\n", err);
        return;
    }
    logger_printf("[MIC] I2S driver and pins configured successfully.\n");
}

size_t read_microphone_data(uint8_t *buffer, size_t buffer_size)
{
    size_t bytes_read = 0;
    if (buffer) {
        i2s_read(I2S_PORT, buffer, buffer_size, &bytes_read, portMAX_DELAY);
    }
    return bytes_read;
}

void deinit_microphone() {
    if (s_i2s_recording_buffer) {
        free(s_i2s_recording_buffer);
        s_i2s_recording_buffer = nullptr;
        logger_printf("[MEM] I2S recording buffer freed.\n");
    }
    if (s_audio_packet_buffer) {
        free(s_audio_packet_buffer);
        s_audio_packet_buffer = nullptr;
        logger_printf("[MEM] Audio packet buffer freed.\n");
    }

    if (i2s_driver_installed) {
        esp_err_t err = i2s_driver_uninstall(I2S_PORT);
        if (err != ESP_OK) {
            logger_printf("[MIC] ERROR: Failed to uninstall I2S driver! Code: 0x%x\n", err);
        } else {
            logger_printf("[MIC] I2S driver uninstalled successfully.\n");
        }
        i2s_driver_installed = false;
    } else {
        logger_printf("[MIC] I2S driver was not installed, skipping uninstall.\n");
    }
}

bool is_microphone_initialized() {
    return i2s_driver_installed;
}