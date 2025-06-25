#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "esp_spiffs.h"
#include "esp_task_wdt.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_gatt_common_api.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"
#include "posix_stub.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "esp_pm.h"

static const char *TAG = "ESP32S3_CAMERA";

// Camera pin definitions for XIAO ESP32S3 Sense
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

#define LED_GPIO_NUM      21

// Microphone I2S pins - from Seeed Studio documentation
#define I2S_WS_PIN        42  // Serial clock (BCK)
#define I2S_SD_PIN        41  // Serial data in
#define I2S_PORT          I2S_NUM_0
#define I2S_SAMPLE_RATE   8000   // 8kHz sample rate like reference (telephony standard)
#define I2S_SAMPLE_BITS   16
#define I2S_CHANNELS      1
#define FRAME_SIZE        160    // 160 samples for 8kHz, 20ms frames (standard for telephony)

// BLE Configuration - Match HTML client exactly
#define BLE_SERVICE_UUID           "12345678-1234-1234-1234-123456789abc"
#define CONTROL_CHAR_UUID          "87654321-4321-4321-4321-cba987654321"
#define STATUS_CHAR_UUID           "11111111-2222-3333-4444-555555555555"
#define IMAGE_CHAR_UUID            "22222222-3333-4444-5555-666666666666"
#define FRAME_CONTROL_CHAR_UUID    "44444444-5555-6666-7777-888888888888"
#define AUDIO_CHAR_UUID            "33333333-4444-5555-6666-777777777777"


#define GATTS_NUM_HANDLE_TEST_A     20
#define TEST_DEVICE_NAME            "OpenSidekick"

// Global variables
static bool ble_device_connected = false;
static bool frame_streaming_enabled = false;
static bool audio_streaming_enabled = false;
static bool capture_image_requested = false;  // Flag for async image capture
static float frame_interval = 0.033f;  // 33ms = 30 FPS (aggressive)
static int image_quality = 25;
static framesize_t current_frame_size = FRAMESIZE_QVGA;

// Audio variables
#define AUDIO_BUFFER_SIZE FRAME_SIZE  // Use frame size like reference
static int16_t* audio_buffer = NULL;
static uint8_t* mulaw_buffer = NULL;  // Buffer for μ-law encoded audio
static bool audio_initialized = false;
static bool i2s_driver_installed = false;

// G.711 μ-law encoding functions
// Convert 16-bit linear PCM to 8-bit μ-law (based on reference implementation)
static uint8_t linear_to_mulaw(int16_t pcm_val) {
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

// Task handles
static TaskHandle_t streaming_task_handle = NULL;
static TaskHandle_t audio_task_handle = NULL;

// Semaphores
static SemaphoreHandle_t camera_mutex;

// BLE related variables
static uint16_t gatts_if;
static uint16_t conn_id;
static uint16_t control_handle;
static uint16_t status_handle;
static uint16_t image_handle;
static uint16_t frame_handle;
static uint16_t audio_handle;

#define PROFILE_NUM 1
#define PROFILE_A_APP_ID 0

// Struct definition for BLE profile instance - must be before array declaration
struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

// Function declarations - moved before struct initialization
static void handle_control_command(const char* command);
static void send_ble_status(void);
static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if_param, esp_ble_gatts_cb_param_t *param);
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void init_microphone(void);
static void audio_task(void *pvParameters);
static void send_audio_data(void);
static void boost_cpu_performance(void);
static void optimize_ble_timing(void);

static struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gatts_cb = gatts_profile_a_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
    },
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0020,
    .max_interval = 0x0040,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 0,  // Disable service UUID in advertising - let scan response handle it
    .p_service_uuid = NULL,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if_param, esp_ble_gatts_cb_param_t *param) 
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(TAG, "REGISTER_APP_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
        
        // TEMPORARY: Use 16-bit UUID for testing - much more reliable for discovery
        gl_profile_tab[PROFILE_A_APP_ID].service_id.is_primary = true;
        gl_profile_tab[PROFILE_A_APP_ID].service_id.id.inst_id = 0x00;
        gl_profile_tab[PROFILE_A_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[PROFILE_A_APP_ID].service_id.id.uuid.uuid.uuid16 = 0x1234;

        esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(TEST_DEVICE_NAME);
        if (set_dev_name_ret){
            ESP_LOGE(TAG, "set device name failed, error code = %x", set_dev_name_ret);
        }
        
        esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
        if (ret){
            ESP_LOGE(TAG, "config adv data failed, error code = %x", ret);
        }
        
        esp_ble_gatts_create_service(gatts_if_param, &gl_profile_tab[PROFILE_A_APP_ID].service_id, GATTS_NUM_HANDLE_TEST_A);
        break;
        
    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(TAG, "CREATE_SERVICE_EVT, status %d, service_handle %d", param->create.status, param->create.service_handle);
        gl_profile_tab[PROFILE_A_APP_ID].service_handle = param->create.service_handle;
        
        // Log the service UUID being created
        ESP_LOGI(TAG, "Service created with UUID: %s", BLE_SERVICE_UUID);
        ESP_LOGI(TAG, "Service handle: %d", param->create.service_handle);
        
        esp_err_t start_ret = esp_ble_gatts_start_service(gl_profile_tab[PROFILE_A_APP_ID].service_handle);
        if (start_ret == ESP_OK) {
            ESP_LOGI(TAG, "Service started successfully");
        } else {
            ESP_LOGE(TAG, "Failed to start service: %s", esp_err_to_name(start_ret));
        }
        
        // Create control characteristic (128-bit UUID)
        esp_bt_uuid_t control_uuid;
        control_uuid.len = ESP_UUID_LEN_128;
        // UUID: 87654321-4321-4321-4321-cba987654321 (little-endian byte order)
        uint8_t control_uuid_128[16] = {
            0x21, 0x43, 0x65, 0x87, 0xa9, 0xcb, 0x21, 0x43,
            0x21, 0x43, 0x21, 0x43, 0x21, 0x43, 0x65, 0x87
        };
        memcpy(control_uuid.uuid.uuid128, control_uuid_128, 16);
        
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &control_uuid,
                              ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                              ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
                              NULL, NULL);
        
        ESP_LOGI(TAG, "Control characteristic creation initiated");
        break;
        
    case ESP_GATTS_START_EVT:
        ESP_LOGI(TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status, param->start.service_handle);
        if (param->start.status == ESP_GATT_OK) {
            ESP_LOGI(TAG, "✅ BLE Service is now ACTIVE and discoverable!");
            ESP_LOGI(TAG, "Service UUID: %s", BLE_SERVICE_UUID);
            ESP_LOGI(TAG, "Device name: %s", TEST_DEVICE_NAME);
        } else {
            ESP_LOGE(TAG, "❌ Failed to start BLE service");
        }
        break;
        
    case ESP_GATTS_ADD_CHAR_EVT:
        ESP_LOGI(TAG, "ADD_CHAR_EVT, status %d, attr_handle %d, service_handle %d",
                param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
        
        // Store the first characteristic handle as control handle
        static int char_count = 0;
        char_count++;
        
        if (char_count == 1) {
        control_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Control characteristic added, handle: %d", control_handle);
            
            // Add status characteristic (128-bit UUID)
            esp_bt_uuid_t status_uuid;
            status_uuid.len = ESP_UUID_LEN_128;
            // UUID: 11111111-2222-3333-4444-555555555555 (little-endian byte order)
            uint8_t status_uuid_128[16] = {
                0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x44, 0x44,
                0x33, 0x33, 0x22, 0x22, 0x11, 0x11, 0x11, 0x11
            };
            memcpy(status_uuid.uuid.uuid128, status_uuid_128, 16);
            
            esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &status_uuid,
                                  ESP_GATT_PERM_READ,
                                  ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                  NULL, NULL);
            
            // Add descriptor immediately for status characteristic
            esp_bt_uuid_t status_descr_uuid;
            status_descr_uuid.len = ESP_UUID_LEN_16;
            status_descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
            esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &status_descr_uuid,
                                        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
        }
        else if (char_count == 2) {
            status_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Status characteristic added, handle: %d", status_handle);
            
            // Add image characteristic (128-bit UUID)
            esp_bt_uuid_t image_uuid;
            image_uuid.len = ESP_UUID_LEN_128;
            // UUID: 22222222-3333-4444-5555-666666666666 (little-endian byte order)
            uint8_t image_uuid_128[16] = {
                0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x55, 0x55,
                0x44, 0x44, 0x33, 0x33, 0x22, 0x22, 0x22, 0x22
            };
            memcpy(image_uuid.uuid.uuid128, image_uuid_128, 16);
            
            esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &image_uuid,
                                  ESP_GATT_PERM_READ,
                                  ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                  NULL, NULL);
            
            // Add descriptor immediately for image characteristic
            esp_bt_uuid_t image_descr_uuid;
            image_descr_uuid.len = ESP_UUID_LEN_16;
            image_descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
            esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &image_descr_uuid,
                                        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
        }
        else if (char_count == 3) {
            image_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Image characteristic added, handle: %d", image_handle);
            
            // Add frame control characteristic (128-bit UUID)
            esp_bt_uuid_t frame_uuid;
            frame_uuid.len = ESP_UUID_LEN_128;
            // UUID: 44444444-5555-6666-7777-888888888888 (little-endian byte order)
            uint8_t frame_uuid_128[16] = {
                0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x77, 0x77,
                0x66, 0x66, 0x55, 0x55, 0x44, 0x44, 0x44, 0x44
            };
            memcpy(frame_uuid.uuid.uuid128, frame_uuid_128, 16);
            
            esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &frame_uuid,
                                  ESP_GATT_PERM_READ,
                                  ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                  NULL, NULL);
            
            // Add descriptor immediately for frame characteristic
            esp_bt_uuid_t frame_descr_uuid;
            frame_descr_uuid.len = ESP_UUID_LEN_16;
            frame_descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
            esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &frame_descr_uuid,
                                        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
        }
        else if (char_count == 4) {
            frame_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Frame control characteristic added, handle: %d", frame_handle);
            
            // Add audio characteristic (128-bit UUID)
            esp_bt_uuid_t audio_uuid;
            audio_uuid.len = ESP_UUID_LEN_128;
            // UUID: 33333333-4444-5555-6666-777777777777 (little-endian byte order)
            uint8_t audio_uuid_128[16] = {
                0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x66, 0x66,
                0x55, 0x55, 0x44, 0x44, 0x33, 0x33, 0x33, 0x33
            };
            memcpy(audio_uuid.uuid.uuid128, audio_uuid_128, 16);
            
            esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &audio_uuid,
                                  ESP_GATT_PERM_READ,
                                  ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                  NULL, NULL);
            
            // Add descriptor immediately for audio characteristic
            esp_bt_uuid_t audio_descr_uuid;
            audio_descr_uuid.len = ESP_UUID_LEN_16;
            audio_descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
            esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &audio_descr_uuid,
                                        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
        }
        else if (char_count == 5) {
            audio_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Audio characteristic added, handle: %d", audio_handle);
            ESP_LOGI(TAG, "All BLE characteristics created successfully!");
        }
        
        // Descriptors are now added immediately after each characteristic
        break;
        
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        ESP_LOGI(TAG, "ADD_DESCR_EVT, status %d, attr_handle %d, service_handle %d",
                param->add_char_descr.status, param->add_char_descr.attr_handle, param->add_char_descr.service_handle);
        break;
        
    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(TAG, "GATT_WRITE_EVT, conn_id %d, trans_id %d, handle %d", 
                 (int)param->write.conn_id, (int)param->write.trans_id, (int)param->write.handle);
        
        if (!param->write.is_prep) {
            ESP_LOGI(TAG, "GATT_WRITE_EVT, value len %d", param->write.len);
            
            // Handle control commands
            if (param->write.handle == control_handle) {
                char command[256];
                memcpy(command, param->write.value, param->write.len);
                command[param->write.len] = '\0';
                handle_control_command(command);
                
                // Small delay to ensure command processing completes before response
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }
        
        // Send response with error handling
        esp_err_t resp_ret = esp_ble_gatts_send_response(gatts_if_param, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        if (resp_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to send BLE response: %s", esp_err_to_name(resp_ret));
        }
        break;
        
    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_CONNECT_EVT, conn_id %d", param->connect.conn_id);
        conn_id = param->connect.conn_id;
        gatts_if = gatts_if_param;
        ble_device_connected = true;
        
        // Set maximum MTU for Data Length Extension (DLE)
        esp_err_t mtu_ret = esp_ble_gatt_set_local_mtu(517);
        if (mtu_ret == ESP_OK) {
            ESP_LOGI(TAG, "MTU set to 517 bytes for maximum throughput");
        } else {
            ESP_LOGW(TAG, "Failed to set MTU: %s", esp_err_to_name(mtu_ret));
        }
        
        // Apply BLE timing optimizations after connection
        vTaskDelay(pdMS_TO_TICKS(100)); // Small delay to ensure connection is stable
        optimize_ble_timing();
        
        send_ble_status();
        break;
        
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT, reason = %d", param->disconnect.reason);
        ble_device_connected = false;
        esp_ble_gap_start_advertising(&adv_params);
        break;
        
    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
        if (param->mtu.mtu == 517) {
            ESP_LOGI(TAG, "✅ Maximum MTU (517) negotiated successfully for DLE");
        } else {
            ESP_LOGI(TAG, "MTU negotiated: %d bytes (max possible with this client)", param->mtu.mtu);
        }
        break;
        
    default:
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if_param, esp_ble_gatts_cb_param_t *param)
{
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gatts_if = gatts_if_param;
        } else {
            ESP_LOGI(TAG, "Reg app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
            return;
        }
    }

    for (int idx = 0; idx < PROFILE_NUM; idx++) {
        if (gatts_if_param == ESP_GATT_IF_NONE || gatts_if_param == gl_profile_tab[idx].gatts_if) {
            if (gl_profile_tab[idx].gatts_cb) {
                gl_profile_tab[idx].gatts_cb(event, gatts_if_param, param);
            }
        }
    }
}

static void handle_control_command(const char* command)
{
    ESP_LOGI(TAG, "Received command: %s", command);
    
    if (strcmp(command, "CAPTURE") == 0) {
        ESP_LOGI(TAG, "Camera capture requested - setting async flag");
        capture_image_requested = true;
    }
    else if (strcmp(command, "START_FRAMES") == 0) {
        frame_streaming_enabled = true;
        ESP_LOGI(TAG, "Frame streaming started");
    }
    else if (strcmp(command, "STOP_FRAMES") == 0) {
        frame_streaming_enabled = false;
        ESP_LOGI(TAG, "Frame streaming stopped");
    }
    else if (strcmp(command, "START_AUDIO") == 0) {
        audio_streaming_enabled = true;
        ESP_LOGI(TAG, "Audio streaming started");
    }
    else if (strcmp(command, "STOP_AUDIO") == 0) {
        audio_streaming_enabled = false;
        ESP_LOGI(TAG, "Audio streaming stopped");
    }
    else if (strncmp(command, "INTERVAL:", 9) == 0) {
        frame_interval = atof(command + 9);
        frame_interval = fmaxf(0.1f, fminf(60.0f, frame_interval));
        ESP_LOGI(TAG, "Frame interval set to %.2f seconds", frame_interval);
    }
    else if (strncmp(command, "QUALITY:", 8) == 0) {
        image_quality = atoi(command + 8);
        image_quality = (image_quality < 4) ? 4 : (image_quality > 63) ? 63 : image_quality;
        sensor_t* s = esp_camera_sensor_get();
        if (s) {
            s->set_quality(s, image_quality);
        }
        ESP_LOGI(TAG, "Image quality set to %d", image_quality);
    }
    else if (strncmp(command, "SIZE:", 5) == 0) {
        int size_value = atoi(command + 5);
        framesize_t new_frame_size;
        
        // Map size values to frame sizes
        switch (size_value) {
            case 0: new_frame_size = FRAMESIZE_96X96; break;    // 96x96
            case 1: new_frame_size = FRAMESIZE_QQVGA; break;    // 160x120
            case 2: new_frame_size = FRAMESIZE_QCIF; break;     // 176x144
            case 3: new_frame_size = FRAMESIZE_HQVGA; break;    // 240x176
            case 4: new_frame_size = FRAMESIZE_240X240; break;  // 240x240
            case 5: new_frame_size = FRAMESIZE_QVGA; break;     // 320x240
            case 6: new_frame_size = FRAMESIZE_CIF; break;      // 400x296
            case 7: new_frame_size = FRAMESIZE_HVGA; break;     // 480x320
            case 8: new_frame_size = FRAMESIZE_VGA; break;      // 640x480
            case 9: new_frame_size = FRAMESIZE_SVGA; break;     // 800x600
            case 10: new_frame_size = FRAMESIZE_XGA; break;     // 1024x768
            case 11: new_frame_size = FRAMESIZE_HD; break;      // 1280x720
            case 12: new_frame_size = FRAMESIZE_SXGA; break;    // 1280x1024
            case 13: new_frame_size = FRAMESIZE_UXGA; break;    // 1600x1200
            default: 
                ESP_LOGW(TAG, "Invalid size value: %d, keeping current size", size_value);
                return;
        }
        
        sensor_t* s = esp_camera_sensor_get();
        if (s) {
            esp_err_t res = s->set_framesize(s, new_frame_size);
            if (res == ESP_OK) {
                current_frame_size = new_frame_size;
                ESP_LOGI(TAG, "Frame size changed to %d (%s)", size_value, 
                    (size_value == 0) ? "96x96" :
                    (size_value == 1) ? "160x120" :
                    (size_value == 2) ? "176x144" :
                    (size_value == 3) ? "240x176" :
                    (size_value == 4) ? "240x240" :
                    (size_value == 5) ? "320x240" :
                    (size_value == 6) ? "400x296" :
                    (size_value == 7) ? "480x320" :
                    (size_value == 8) ? "640x480" :
                    (size_value == 9) ? "800x600" :
                    (size_value == 10) ? "1024x768" :
                    (size_value == 11) ? "1280x720" :
                    (size_value == 12) ? "1280x1024" :
                    (size_value == 13) ? "1600x1200" : "Unknown");
            } else {
                ESP_LOGE(TAG, "Failed to set frame size to %d: %s", size_value, esp_err_to_name(res));
            }
        } else {
            ESP_LOGE(TAG, "Camera sensor not available");
        }
    }
    else if (strcmp(command, "STATUS") == 0) {
        send_ble_status();
    }
}

static void send_image_chunks(uint8_t* image_data, size_t image_len, uint16_t char_handle, bool is_frame)
{
    if (!ble_device_connected || !image_data || image_len == 0) return;
    
    const size_t max_chunk_size = 510; // Increased from 490 to 510 bytes (MTU 517 - 7 header bytes)
    const size_t header_size = 7;      // Header: [type][chunks_hi][chunks_lo][size_b0][size_b1][size_b2][size_b3]
    
    // Calculate total chunks needed
    size_t total_chunks = (image_len + max_chunk_size - 1) / max_chunk_size;
    
    ESP_LOGI(TAG, "Sending %s: %zu bytes in %zu chunks", 
             is_frame ? "frame" : "image", image_len, total_chunks);
    
    // Send start header with 32-bit size
    uint8_t start_header[header_size];
    start_header[0] = 0x01; // Start marker
    start_header[1] = (total_chunks >> 8) & 0xFF;
    start_header[2] = total_chunks & 0xFF;
    start_header[3] = (image_len >> 0) & 0xFF;
    start_header[4] = (image_len >> 8) & 0xFF;
    start_header[5] = (image_len >> 16) & 0xFF;
    start_header[6] = (image_len >> 24) & 0xFF;
    
    esp_err_t header_ret = esp_ble_gatts_send_indicate(gatts_if, conn_id, char_handle, header_size, start_header, false);
    if (header_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send header: %s", esp_err_to_name(header_ret));
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(1)); // Minimal delay to ensure header is processed before chunks
    
    // Send data chunks
    size_t successful_chunks = 0;
    for (size_t chunk_idx = 0; chunk_idx < total_chunks; chunk_idx++) {
        size_t offset = chunk_idx * max_chunk_size;
        size_t chunk_size = (offset + max_chunk_size > image_len) ? 
                           (image_len - offset) : max_chunk_size;
        
        // Create chunk with header: [0x02][chunk_idx_hi][chunk_idx_lo][data...]
        uint8_t* chunk_packet = heap_caps_malloc(3 + max_chunk_size, MALLOC_CAP_8BIT);
        if (!chunk_packet) {
            ESP_LOGE(TAG, "Failed to allocate chunk buffer");
            return;
        }
        
        chunk_packet[0] = 0x02; // Data chunk marker
        chunk_packet[1] = (chunk_idx >> 8) & 0xFF;
        chunk_packet[2] = chunk_idx & 0xFF;
        
        memcpy(&chunk_packet[3], &image_data[offset], chunk_size);
        
        esp_err_t ret = esp_ble_gatts_send_indicate(gatts_if, conn_id, char_handle, 3 + chunk_size, chunk_packet, false);
        if (ret == ESP_OK) {
            successful_chunks++;
        } else {
            ESP_LOGW(TAG, "Failed to send chunk %zu: %s", chunk_idx, esp_err_to_name(ret));
        }
        
        free(chunk_packet);
        
        // Balanced delay for high throughput while maintaining command responsiveness
        vTaskDelay(pdMS_TO_TICKS(1));  // 1ms delay to allow BLE command processing
        
        // Progress logging every 10 chunks
        if ((chunk_idx + 1) % 10 == 0) {
            ESP_LOGI(TAG, "Progress: %zu/%zu chunks sent successfully", successful_chunks, chunk_idx + 1);
        }
    }
    
    // Send end marker
    uint8_t end_header[3];
    end_header[0] = 0x03; // End marker
    end_header[1] = (total_chunks >> 8) & 0xFF;
    end_header[2] = total_chunks & 0xFF;
    
    esp_err_t end_ret = esp_ble_gatts_send_indicate(gatts_if, conn_id, char_handle, 3, end_header, false);
    if (end_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send end marker: %s", esp_err_to_name(end_ret));
    }
    
    ESP_LOGI(TAG, "Transmission complete: %zu/%zu chunks successful for %s (%zu bytes)", 
             successful_chunks, total_chunks, is_frame ? "frame" : "image", image_len);
}

static void send_ble_status(void)
{
    if (!ble_device_connected) return;
    
    char status[512];
    int battery_level = 50; // Mock battery level
    
    snprintf(status, sizeof(status),
        "{"
        "\"ble\":%s,"
        "\"frames\":%s,"
        "\"audio\":%s,"
        "\"interval\":%.2f,"
        "\"quality\":%d,"
        "\"size\":%d,"
        "\"battery\":%d,"
        "\"free_heap\":%zu"
        "}",
        ble_device_connected ? "true" : "false",
        frame_streaming_enabled ? "true" : "false",
        audio_streaming_enabled ? "true" : "false",
        frame_interval,
        image_quality,
        current_frame_size,
        battery_level,
        heap_caps_get_free_size(MALLOC_CAP_8BIT)
    );
    
    ESP_LOGI(TAG, "Status: %s", status);
}

static void init_ble(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret){
        ESP_LOGE(TAG, "gatts register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret){
        ESP_LOGE(TAG, "gap register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gatts_app_register(PROFILE_A_APP_ID);
    if (ret){
        ESP_LOGE(TAG, "gatts app register error, error code = %x", ret);
        return;
    }

    // Service UUID is now statically initialized as 16-bit UUID

    ESP_LOGI(TAG, "BLE server started and advertising...");
    ESP_LOGI(TAG, "Device name: %s", TEST_DEVICE_NAME);
    ESP_LOGI(TAG, "Service UUID: %s", BLE_SERVICE_UUID);
}

static void streaming_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Streaming task started on core %d", xPortGetCoreID());
    ESP_LOGI(TAG, "Task stack size: %d bytes", uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t));
    
    TickType_t last_frame_time = 0;
    uint32_t frame_count = 0;
    uint32_t failed_frames = 0;
    
    while (true) {
        TickType_t current_time = xTaskGetTickCount();
        
        // Handle continuous frame streaming with frame rate control
        if (frame_streaming_enabled && ble_device_connected) {
            // Control frame rate based on frame_interval
            if (current_time - last_frame_time >= pdMS_TO_TICKS((int)(frame_interval * 1000))) {
                
                // Capture frame from camera
                camera_fb_t *fb = esp_camera_fb_get();
                if (!fb) {
                    ESP_LOGW(TAG, "Camera capture failed");
                    failed_frames++;
                    last_frame_time = current_time;
                    vTaskDelay(pdMS_TO_TICKS(20)); // Faster retry
                    continue;
                }
                
                ESP_LOGI(TAG, "Captured streaming frame %lu: %zu bytes (%dx%d)", 
                        frame_count, fb->len, fb->width, fb->height);
                
                // Send JPEG image for streaming
                send_image_chunks(fb->buf, fb->len, frame_handle, true);
                
                esp_camera_fb_return(fb);
                last_frame_time = current_time;
                frame_count++;
                
                // Conservative task management like Arduino project
                if (frame_count % 10 == 0) {
                    ESP_LOGI(TAG, "Streaming stats: %lu frames, %lu failed, free heap: %zu", 
                            frame_count, failed_frames, heap_caps_get_free_size(MALLOC_CAP_8BIT));
                    
                    // Stack monitoring like Arduino watchdog approach
                    UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(NULL);
                    if (stack_remaining < 1000) {  // Warning if less than 1KB stack remaining
                        ESP_LOGW(TAG, "Low stack warning: %d bytes remaining", stack_remaining * sizeof(StackType_t));
                    }
                }
            }
        }
        
        // Handle single capture requests
        if (capture_image_requested && ble_device_connected) {
            capture_image_requested = false; // Reset flag
            ESP_LOGI(TAG, "Processing single capture request");
            
            // Capture frame from camera
            camera_fb_t *fb = esp_camera_fb_get();
            if (!fb) {
                ESP_LOGW(TAG, "Camera capture failed for single image");
            } else {
                ESP_LOGI(TAG, "Captured single image: %zu bytes (%dx%d)", 
                        fb->len, fb->width, fb->height);
                
                // Send JPEG image for single capture
                send_image_chunks(fb->buf, fb->len, image_handle, false);
                
                esp_camera_fb_return(fb);
            }
        }
        
        // Conservative delay to prevent overwhelming system
        vTaskDelay(pdMS_TO_TICKS(10)); // Reduced from 20ms to 10ms for higher frame rate capability
    }
}

static void init_camera(void)
{
    camera_config_t camera_config = {
        .pin_pwdn = PWDN_GPIO_NUM,
        .pin_reset = RESET_GPIO_NUM,
        .pin_xclk = XCLK_GPIO_NUM,
        .pin_sscb_sda = SIOD_GPIO_NUM,
        .pin_sscb_scl = SIOC_GPIO_NUM,
        .pin_d7 = Y9_GPIO_NUM,
        .pin_d6 = Y8_GPIO_NUM,
        .pin_d5 = Y7_GPIO_NUM,
        .pin_d4 = Y6_GPIO_NUM,
        .pin_d3 = Y5_GPIO_NUM,
        .pin_d2 = Y4_GPIO_NUM,
        .pin_d1 = Y3_GPIO_NUM,
        .pin_d0 = Y2_GPIO_NUM,
        .pin_vsync = VSYNC_GPIO_NUM,
        .pin_href = HREF_GPIO_NUM,
        .pin_pclk = PCLK_GPIO_NUM,
        .xclk_freq_hz = 24000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,  // JPEG for direct image sending
        .frame_size = FRAMESIZE_QVGA,      // QVGA: 320x240 for viewable images
        .jpeg_quality = 12,                // High quality JPEG
        .fb_count = 1,                     // Single buffer for faster processing
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,  // Same as Arduino project
        .fb_location = CAMERA_FB_IN_PSRAM     // Explicit PSRAM usage like Arduino
    };

    ESP_LOGI(TAG, "Available PSRAM: %zu bytes", esp_psram_get_size());
    ESP_LOGI(TAG, "Free PSRAM: %zu bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "Free heap: %zu bytes", heap_caps_get_free_size(MALLOC_CAP_8BIT));

    // First attempt - QVGA resolution
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        ESP_LOGI(TAG, "Trying smaller frame size...");
        
        // Fallback to smaller frame size like Arduino project
        camera_config.frame_size = FRAMESIZE_QQVGA;  // 160x120
        camera_config.jpeg_quality = 20;
        
        err = esp_camera_init(&camera_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Camera fallback init also failed with error 0x%x", err);
            ESP_LOGI(TAG, "Trying minimal configuration...");
            
            // Final fallback - minimal config like Arduino
            camera_config.frame_size = FRAMESIZE_96X96;  // 96x96
            camera_config.jpeg_quality = 30;
            
            err = esp_camera_init(&camera_config);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Camera minimal init failed with error 0x%x", err);
                ESP_LOGI(TAG, "Camera not available - continuing without camera");
                return;
                    }
                }
            }
            
    sensor_t* s = esp_camera_sensor_get();
    if (s != NULL) {
        // Set conservative camera settings optimized for BLE like Arduino
        s->set_framesize(s, FRAMESIZE_QVGA);   // 320x240 conservative size
        s->set_quality(s, image_quality);
        s->set_brightness(s, 0);     // -2 to 2
        s->set_contrast(s, 0);       // -2 to 2  
        s->set_saturation(s, 0);     // -2 to 2
        s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
        s->set_colorbar(s, 0);       // 0 = disable, 1 = enable
        s->set_whitebal(s, 1);       // 0 = disable, 1 = enable
        s->set_gain_ctrl(s, 1);      // 0 = disable, 1 = enable
        s->set_exposure_ctrl(s, 1);  // 0 = disable, 1 = enable
        s->set_hmirror(s, 0);        // 0 = disable, 1 = enable
        s->set_vflip(s, 0);          // 0 = disable, 1 = enable
        
        ESP_LOGI(TAG, "Camera initialized successfully with JPEG format");
        current_frame_size = FRAMESIZE_QVGA;
                } else {
        ESP_LOGE(TAG, "Failed to get camera sensor");
    }
}

static void init_microphone(void)
{
    ESP_LOGI(TAG, "Initializing PDM microphone with G.711 μ-law encoding...");
    ESP_LOGI(TAG, "PDM pins: CLK=%d, DIN=%d", I2S_WS_PIN, I2S_SD_PIN);
    
    // Allocate audio buffer
    audio_buffer = heap_caps_malloc(AUDIO_BUFFER_SIZE * sizeof(int16_t), MALLOC_CAP_8BIT);
    if (!audio_buffer) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        return;
    }
    
    // Allocate μ-law encoded buffer (1 byte per sample instead of 2)
    mulaw_buffer = heap_caps_malloc(AUDIO_BUFFER_SIZE, MALLOC_CAP_8BIT);
    if (!mulaw_buffer) {
        ESP_LOGE(TAG, "Failed to allocate μ-law buffer");
        free(audio_buffer);
        return;
    }
    
    ESP_LOGI(TAG, "Audio buffers allocated: PCM=%p (%d bytes), μ-law=%p (%d bytes)", 
             audio_buffer, AUDIO_BUFFER_SIZE * sizeof(int16_t), mulaw_buffer, AUDIO_BUFFER_SIZE);
    
    // PDM configuration for XIAO ESP32S3 Sense microphone
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),  // Enable PDM mode for MEMS microphone
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,  // Higher priority for audio
        .dma_buf_count = 6,  // More buffers for smoother audio
        .dma_buf_len = FRAME_SIZE / 2,  // Smaller buffers for lower latency
        .use_apll = true,  // Use Audio PLL for better timing
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_WS_PIN,      // PDM Clock
        .ws_io_num = -1,               // Not used for PDM
        .data_out_num = -1,            // Not used (RX only)
        .data_in_num = I2S_SD_PIN      // PDM Data
    };

    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2S driver! Code: 0x%x", err);
        i2s_driver_installed = false;
        return;
                } else {
        i2s_driver_installed = true;
        ESP_LOGI(TAG, "I2S PDM driver installed successfully");
    }
    
    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set I2S pins! Code: 0x%x", err);
        return;
    }
    
    // PDM specific settings
    err = i2s_set_pdm_rx_down_sample(I2S_PORT, I2S_PDM_DSR_8S);  // Downsample for better quality
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set PDM downsample! Code: 0x%x", err);
        return;
    }
    
    ESP_LOGI(TAG, "I2S PDM driver and pins configured successfully");
    audio_initialized = true;
    ESP_LOGI(TAG, "PDM microphone with G.711 μ-law encoding initialized successfully");
}

static void send_audio_data(void)
{
    if (!ble_device_connected || !audio_initialized || !audio_buffer || !mulaw_buffer || !i2s_driver_installed) {
        ESP_LOGW(TAG, "Audio send conditions not met: BLE=%d, audio_init=%d, buffer=%p, mulaw=%p, i2s_driver=%d", 
                 ble_device_connected, audio_initialized, audio_buffer, mulaw_buffer, i2s_driver_installed);
        return;
    }
    
    size_t bytes_read = 0;
    // Use old I2S driver API with shorter timeout for PDM
    esp_err_t i2s_ret = i2s_read(I2S_PORT, audio_buffer, AUDIO_BUFFER_SIZE * sizeof(int16_t), &bytes_read, pdMS_TO_TICKS(50));
    
    if (i2s_ret != ESP_OK) {
        ESP_LOGD(TAG, "I2S read failed: %s", esp_err_to_name(i2s_ret));
        return;
    }
    
    if (bytes_read == 0) {
        ESP_LOGD(TAG, "No audio data read from I2S");
        return;
    }
    
    size_t samples_read = bytes_read / sizeof(int16_t);
    
    // Log successful read (but not too frequently)
    static uint32_t read_count = 0;
    read_count++;
    if (read_count % 50 == 1) {  // Log every 50th successful read
        ESP_LOGI(TAG, "Successfully read %zu bytes (%zu samples) from PDM microphone, count: %lu", 
                 bytes_read, samples_read, read_count);
    }
    
    // Improved noise gate with dynamic threshold
    int32_t rms_sum = 0;
    for (size_t i = 0; i < samples_read; i++) {
        rms_sum += (int32_t)audio_buffer[i] * audio_buffer[i];
    }
    int16_t rms_level = sqrt(rms_sum / samples_read);
    
    // Adaptive noise threshold based on recent signal levels
    static int16_t adaptive_threshold = 50;
    if (rms_level > adaptive_threshold * 2) {
        adaptive_threshold = rms_level / 4;  // Adapt to signal level
    }
    
    if (rms_level < adaptive_threshold) {
        ESP_LOGD(TAG, "Audio below adaptive noise threshold (%d), skipping", adaptive_threshold);
        return;
    }
    
    // Encode to μ-law WITHOUT gain amplification to prevent clipping
    size_t mulaw_samples = 0;
    
    for (size_t i = 0; i < samples_read; i++) {
        // Use original sample without gain to prevent distortion
        int16_t sample = audio_buffer[i];
        
        // Optional: Apply simple high-pass filter to remove DC offset
        static int16_t prev_sample = 0;
        int16_t filtered = sample - ((prev_sample * 15) >> 4);  // Simple DC removal
        prev_sample = sample;
        
        // Encode to μ-law directly without amplification
        mulaw_buffer[mulaw_samples++] = linear_to_mulaw(filtered);
    }
    
    ESP_LOGI(TAG, "Sending %zu bytes of G.711 μ-law audio data via BLE (compressed from %zu bytes PCM)", 
             mulaw_samples, bytes_read);
    
    // Send raw μ-law audio data directly (like reference implementation)
    esp_err_t send_ret = esp_ble_gatts_send_indicate(gatts_if, conn_id, audio_handle,
                                               mulaw_samples, mulaw_buffer, false);
    if (send_ret == ESP_OK) {
        ESP_LOGD(TAG, "Successfully sent %zu bytes of μ-law audio", mulaw_samples);
    } else {
        ESP_LOGW(TAG, "Failed to send μ-law audio: %s", esp_err_to_name(send_ret));
    }
    
    // Log transmission summary (but not too frequently)
    if (read_count % 10 == 1) {
        ESP_LOGI(TAG, "μ-law audio transmission: %zu bytes sent, RMS level: %d, threshold: %d", 
                 mulaw_samples, rms_level, adaptive_threshold);
    }
}

static void audio_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Audio task started on core %d", xPortGetCoreID());
    
    TickType_t last_audio_time = 0;
    const TickType_t audio_interval = pdMS_TO_TICKS(100); // Reduced from 125ms to 100ms = 10 audio packets/sec for better bandwidth
    uint32_t audio_attempts = 0;
    
    while (true) {
        TickType_t current_time = xTaskGetTickCount();
        
        if (audio_streaming_enabled && ble_device_connected && audio_initialized) {
            if (current_time - last_audio_time >= audio_interval) {
                audio_attempts++;
                if (audio_attempts % 10 == 1) { // Log every 10th attempt (once per second with 100ms intervals)
                    ESP_LOGI(TAG, "μ-law audio streaming active, attempt #%lu", audio_attempts);
                }
                send_audio_data();
                last_audio_time = current_time;
            }
        } else {
            // Log why audio isn't streaming (but not too frequently)
            if (audio_attempts % 100 == 0) {
                ESP_LOGD(TAG, "Audio not streaming: enabled=%d, ble=%d, init=%d", 
                        audio_streaming_enabled, ble_device_connected, audio_initialized);
            }
            audio_attempts++;
        }
        
        vTaskDelay(pdMS_TO_TICKS(25)); // Slightly longer delay to reduce CPU load
    }
}

static void init_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    ESP_LOGI(TAG, "Mounting SPIFFS...");
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS mounted successfully");
        ESP_LOGI(TAG, "Partition size: total: %zu, used: %zu", total, used);
    }
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&adv_params);
                            break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Advertising start failed");
        } else {
            ESP_LOGI(TAG, "Advertising started successfully");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Advertising stop failed");
    } else {
            ESP_LOGI(TAG, "Stop adv successfully");
        }
        break;
    default:
        break;
    }
}

// Advanced performance optimization functions
static void boost_cpu_performance(void)
{
    // Set CPU to maximum performance mode
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 240,  // Lock to max frequency
        .light_sleep_enable = false  // Disable power saving
    };
    esp_err_t ret = esp_pm_configure(&pm_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "CPU locked to maximum 240MHz performance mode");
    } else {
        ESP_LOGW(TAG, "Failed to set CPU performance mode: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "CPU performance optimizations applied");
}

static void optimize_ble_timing(void)
{
    if (!ble_device_connected) {
        ESP_LOGW(TAG, "Cannot optimize BLE timing - device not connected");
        return;
    }
    
    // Ultra-aggressive connection parameters for maximum throughput
    esp_ble_conn_update_params_t conn_params = {
        .bda = {0}, // Will be filled by the stack
        .min_int = 6,   // 7.5ms minimum interval (fastest stable)
        .max_int = 6,   // 7.5ms maximum interval (no variance)
        .latency = 0,   // No latency for maximum responsiveness
        .timeout = 400  // 4 second timeout
    };
    
    esp_err_t ret = esp_ble_gap_update_conn_params(&conn_params);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "BLE connection parameters optimized: 7.5ms interval, 0 latency");
    } else {
        ESP_LOGW(TAG, "Failed to optimize BLE connection parameters: %s", esp_err_to_name(ret));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "OpenSidekick Camera & Audio Streamer Starting...");
    ESP_LOGI(TAG, "======================================");

    // Initialize POSIX stub functions for H.264 library compatibility
    posix_stub_init();

    // Apply advanced CPU performance optimizations
    boost_cpu_performance();

    // Initialize ESP32 task watchdog with longer timeout for initialization
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 10000,  // Increased to 10 seconds for initialization
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true
    };
    esp_task_wdt_init(&twdt_config);
    esp_task_wdt_add(NULL);

    // Create mutex
    camera_mutex = xSemaphoreCreateMutex();
    if (!camera_mutex) {
        ESP_LOGE(TAG, "Failed to create camera mutex");
        return;
    }

    // Initialize SPIFFS with progress updates
    ESP_LOGI(TAG, "Initializing SPIFFS...");
    esp_task_wdt_reset();  // Reset watchdog before SPIFFS init
    init_spiffs();
    esp_task_wdt_reset();  // Reset watchdog after SPIFFS init
    
    // Initialize camera
    ESP_LOGI(TAG, "Initializing camera...");
    esp_task_wdt_reset();  // Reset watchdog before camera init
    init_camera();
    esp_task_wdt_reset();  // Reset watchdog after camera init
    
    // Initialize microphone
    ESP_LOGI(TAG, "Initializing microphone...");
    esp_task_wdt_reset();  // Reset watchdog before microphone init
    init_microphone();
    esp_task_wdt_reset();  // Reset watchdog after microphone init
    
    // Initialize BLE
    ESP_LOGI(TAG, "Initializing BLE...");
    esp_task_wdt_reset();  // Reset watchdog before BLE init
    init_ble();
    esp_task_wdt_reset();  // Reset watchdog after BLE init
    
    // Create streaming task with large stack for H.264 + BLE operations
    xTaskCreatePinnedToCore(
        streaming_task,
        "streaming_task",
        16384,  // Increased to 16KB for H.264 encoding + BLE chunk transmission
        NULL,
        10,
        &streaming_task_handle,
        1
    );
    
    // Create audio task for microphone handling
    xTaskCreatePinnedToCore(
        audio_task,
        "audio_task",
        8192,   // 8KB stack for audio processing
        NULL,
        9,      // Slightly lower priority than streaming
        &audio_task_handle,
        0       // Run on core 0
    );
    
    ESP_LOGI(TAG, "======================================");
    ESP_LOGI(TAG, "System initialized successfully!");
    ESP_LOGI(TAG, "Device ready for BLE connections...");
    ESP_LOGI(TAG, "Device name: %s", TEST_DEVICE_NAME);
    ESP_LOGI(TAG, "Service UUID: %s", BLE_SERVICE_UUID);
    ESP_LOGI(TAG, "======================================");
    
    // Reset watchdog timeout to normal value after initialization
    twdt_config.timeout_ms = 5000;  // Back to 5 seconds for normal operation
    esp_task_wdt_init(&twdt_config);
    
    // Main task just handles watchdog
    while (1) {
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}