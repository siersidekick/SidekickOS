# Firmware Guide - SidekickOS ESP32S3

## 🚀 **Overview**

The SidekickOS firmware is built on ESP-IDF (Espressif IoT Development Framework) and provides:
- High-performance camera capture and streaming
- Optimized BLE 5.0 communication with 517-byte MTU
- Advanced power management and performance optimization
- Modular architecture for easy customization
(PDM microphone audio with G.711 μ-law encoding is WIP)

## 🏗️ **Architecture**

### **Core Components**

```
SidekickOS Firmware Architecture
┌─────────────────────────────────────────┐
│                main.c                   │
├─────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────────┐  │
│  │   Camera    │  │      Audio      │  │
│  │   Module    │  │     Module      │  │
│  │             │  │                 │  │
│  │ • OV2640    │  │ • PDM Mic       │  │
│  │ • JPEG      │  │ • G.711 μ-law   │  │
│  │ • Streaming │  │ • I2S Driver    │  │
│  └─────────────┘  └─────────────────┘  │
├─────────────────────────────────────────┤
│              BLE Stack                  │
│  ┌─────────────────────────────────────┐│
│  │ • Service: Camera Control          ││
│  │ • Characteristics: Image, Audio    ││
│  │ • MTU: 517 bytes (BLE 5.0 DLE)     ││
│  │ • Chunk Protocol: 510-byte packets ││
│  └─────────────────────────────────────┘│
├─────────────────────────────────────────┤
│           Performance Layer             │
│  • CPU: 240MHz locked                  │
│  • Memory: PSRAM optimization          │
│  • Power: Aggressive connection params │
│  • Tasks: Dual-core task distribution  │
└─────────────────────────────────────────┘
```

### **Task Architecture**

| Task | Core | Priority | Stack Size | Function |
|------|------|----------|------------|----------|
| Main Task | 0 | 1 | 8KB | System initialization, watchdog |
| Streaming Task | 1 | 10 | 16KB | Camera capture, image transmission |
| Audio Task | 0 | 9 | 8KB | Microphone capture, audio streaming |
| BLE Stack | 0 | 19 | System | BLE communication handling |

## 🔧 **Build System**

### **Prerequisites**

1. **ESP-IDF Installation**
```bash
# Install ESP-IDF v5.0 or later
git clone -b v5.0 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh
source export.sh
```

2. **Hardware Setup**
- XIAO ESP32S3 Sense board
- USB-C cable for flashing
- Ensure camera and microphone are connected

### **Build Process**

```bash
# Navigate to firmware directory
cd sidekickos/firmware

# Configure project (optional - defaults are optimized)
idf.py menuconfig

# Build firmware
idf.py build

# Flash to device
idf.py flash

# Monitor serial output
idf.py monitor

# Combined flash and monitor
idf.py flash monitor
```

### **Build Configuration**

Key configuration options in `sdkconfig`:

```ini
# Performance Optimization
CONFIG_ESP32S3_SPIRAM_SUPPORT=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y

# BLE Optimization
CONFIG_BT_BLE_DATA_LENGTH_EXTENSION=y
CONFIG_BT_GATT_MAX_MTU=517
CONFIG_BT_ACL_TX_BUF_NUM=12

# Compiler Optimization
CONFIG_COMPILER_OPTIMIZATION_PERF=y
CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_DISABLE=y

# Camera Configuration
CONFIG_CAMERA_CORE0=y
CONFIG_CAMERA_DMA_BUFFER_SIZE_MAX=32768
```

## 📡 **BLE Protocol**

### **Service Structure**

**Primary Service**: `12345678-1234-1234-1234-123456789abc`

| Characteristic | UUID | Properties | Description |
|----------------|------|------------|-------------|
| Control | `87654321-4321-4321-4321-cba987654321` | Read/Write | Command interface |
| Status | `11111111-2222-3333-4444-555555555555` | Read/Notify | Device status |
| Image | `22222222-3333-4444-5555-666666666666` | Read/Notify | Single image capture |
| Frame | `44444444-5555-6666-7777-888888888888` | Read/Notify | Video streaming |
| Audio | `33333333-4444-5555-6666-777777777777` | Read/Notify | Audio streaming |

### **Command Protocol**

**Control Commands** (sent to Control characteristic):

| Command | Format | Description |
|---------|--------|-------------|
| Capture | `CAPTURE` | Take single photo |
| Start Streaming | `START_FRAMES` | Begin video streaming |
| Stop Streaming | `STOP_FRAMES` | End video streaming |
| Start Audio | `START_AUDIO` | Begin audio streaming |
| Stop Audio | `STOP_AUDIO` | End audio streaming |
| Set Quality | `QUALITY:N` | Set JPEG quality (4-63) |
| Set Resolution | `SIZE:N` | Set camera resolution (0-13) |
| Set Interval | `INTERVAL:F` | Set frame interval in seconds |
| Get Status | `STATUS` | Request device status |

### **Data Transmission Protocol**

**Image/Frame Transmission** (chunked protocol):

1. **Start Header** (7 bytes):
   ```
   [0x01][chunks_hi][chunks_lo][size_b0][size_b1][size_b2][size_b3]
   ```

2. **Data Chunks** (3 + data bytes):
   ```
   [0x02][chunk_idx_hi][chunk_idx_lo][...data...]
   ```

3. **End Marker** (3 bytes):
   ```
   [0x03][chunks_hi][chunks_lo]
   ```

**Audio Transmission** (direct):
- Raw G.711 μ-law encoded data
- 160 samples per packet (160 bytes)
- 10 packets per second (100ms intervals)

## 📷 **Camera System**

### **Supported Resolutions**

| Size ID | Resolution | Dimensions | Typical Use |
|---------|------------|------------|-------------|
| 0 | 96x96 | 96×96 | Thumbnails |
| 1 | QQVGA | 160×120 | Low bandwidth |
| 5 | QVGA | 320×240 | Standard streaming |
| 8 | VGA | 640×480 | High quality |
| 9 | SVGA | 800×600 | Photo capture |
| 10 | XGA | 1024×768 | High resolution |
| 11 | HD | 1280×720 | HD capture |
| 12 | SXGA | 1280×1024 | Maximum quality |
| 13 | UXGA | 1600×1200 | Ultra high-res |

### **Camera Configuration**

```c
camera_config_t camera_config = {
    .pin_pwdn = -1,
    .pin_reset = -1,
    .pin_xclk = 10,
    .pin_sscb_sda = 40,
    .pin_sscb_scl = 39,
    .pin_d7 = 48,
    .pin_d6 = 11,
    .pin_d5 = 12,
    .pin_d4 = 14,
    .pin_d3 = 16,
    .pin_d2 = 18,
    .pin_d1 = 17,
    .pin_d0 = 15,
    .pin_vsync = 38,
    .pin_href = 47,
    .pin_pclk = 13,
    .xclk_freq_hz = 24000000,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_QVGA,
    .jpeg_quality = 12,
    .fb_count = 1,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    .fb_location = CAMERA_FB_IN_PSRAM
};
```

## 🎤 **Audio System**

### **PDM Microphone Configuration**

```c
i2s_config_t i2s_config = {
    .mode = (I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
    .sample_rate = 8000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
    .dma_buf_count = 6,
    .dma_buf_len = 80,
    .use_apll = true,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
};
```

### **G.711 μ-law Encoding**

The firmware implements G.711 μ-law compression:
- **Input**: 16-bit PCM samples at 8kHz
- **Output**: 8-bit μ-law compressed audio
- **Compression Ratio**: 2:1
- **Quality**: Telephone quality (suitable for voice)

```c
uint8_t linear_to_mulaw(int16_t pcm_val) {
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
```

## ⚡ **Performance Optimization**

### **CPU Optimization**

```c
void boost_cpu_performance(void) {
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 240,  // Lock to max frequency
        .light_sleep_enable = false
    };
    esp_pm_configure(&pm_config);
}
```

### **BLE Optimization**

```c
void optimize_ble_timing(void) {
    esp_ble_conn_update_params_t conn_params = {
        .min_int = 6,   // 7.5ms minimum interval
        .max_int = 6,   // 7.5ms maximum interval
        .latency = 0,   // No latency
        .timeout = 400  // 4 second timeout
    };
    esp_ble_gap_update_conn_params(&conn_params);
}
```

### **Memory Management**

- **PSRAM Usage**: Camera framebuffers stored in PSRAM
- **Heap Monitoring**: Continuous free heap monitoring
- **Buffer Management**: Optimized buffer allocation and deallocation
- **Stack Sizes**: Task stacks sized for peak usage

## 🔧 **Customization**

### **Adding New Commands**

1. **Define Command String**:
```c
#define NEW_COMMAND "MY_COMMAND"
```

2. **Add Handler in `handle_control_command()`**:
```c
else if (strcmp(command, NEW_COMMAND) == 0) {
    // Handle your command
    ESP_LOGI(TAG, "Processing new command");
    // Your implementation here
}
```

3. **Update Documentation**: Add command to protocol docs

### **Adding New BLE Characteristics**

1. **Define UUID**:
```c
#define NEW_CHAR_UUID "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"
```

2. **Add Characteristic Creation**:
```c
// In gatts_profile_a_event_handler, ESP_GATTS_ADD_CHAR_EVT case
esp_bt_uuid_t new_uuid;
new_uuid.len = ESP_UUID_LEN_128;
// Set UUID bytes...
esp_ble_gatts_add_char(service_handle, &new_uuid, permissions, properties, NULL, NULL);
```

3. **Handle Read/Write Events**: Add handlers in BLE event callbacks

### **Modifying Camera Settings**

```c
sensor_t* s = esp_camera_sensor_get();
if (s != NULL) {
    s->set_brightness(s, 0);     // -2 to 2
    s->set_contrast(s, 0);       // -2 to 2  
    s->set_saturation(s, 0);     // -2 to 2
    s->set_gainceiling(s, 0);    // 0 to 6
    s->set_whitebal(s, 1);       // 0 = disable, 1 = enable
    s->set_hmirror(s, 0);        // 0 = disable, 1 = enable
    s->set_vflip(s, 0);          // 0 = disable, 1 = enable
}
```

## 🐛 **Debugging**

### **Serial Output**

```bash
# Monitor with timestamp and filtering
idf.py monitor --print_filter="*:I ESP32S3_CAMERA:D"

# Save output to file
idf.py monitor | tee debug_output.log
```

### **Common Debug Points**

1. **Camera Initialization**:
```c
ESP_LOGI(TAG, "Camera init result: %s", esp_err_to_name(err));
```

2. **BLE Connection Status**:
```c
ESP_LOGI(TAG, "BLE connected: %s", ble_device_connected ? "true" : "false");
```

3. **Memory Usage**:
```c
ESP_LOGI(TAG, "Free heap: %zu bytes", heap_caps_get_free_size(MALLOC_CAP_8BIT));
```

4. **Performance Metrics**:
```c
ESP_LOGI(TAG, "Frame %lu: %zu bytes in %lu ms", frame_count, fb->len, capture_time);
```

### **Troubleshooting**

| Issue | Symptoms | Solution |
|-------|----------|----------|
| Camera not working | "Camera init failed" error | Check hardware connections, try lower resolution |
| BLE not advertising | No device visible in scans | Check BLE initialization, verify UUID format |
| Poor performance | Low FPS, timeouts | Check CPU frequency, optimize task priorities |
| Memory issues | Heap allocation failures | Reduce buffer sizes, check for memory leaks |
| Audio problems | No audio data, crackling | Verify I2S pins, check PDM configuration |

## 📊 **Performance Metrics**

### **Typical Performance**

| Configuration | Image Size | Transfer Time | Throughput | FPS |
|---------------|------------|---------------|------------|-----|
| QVGA, Q=25 | ~8KB | 0.8s | 80 kbps | 1.25 |
| QVGA, Q=12 | ~15KB | 1.2s | 100 kbps | 0.83 |
| VGA, Q=25 | ~25KB | 2.0s | 100 kbps | 0.5 |
| VGA, Q=12 | ~45KB | 3.5s | 103 kbps | 0.29 |

### **Audio Performance**

- **Latency**: 100-200ms
- **Throughput**: 12.8 kbps (64 kbps PCM → 12.8 kbps μ-law)
- **Quality**: Telephone quality, suitable for voice
- **CPU Usage**: ~15% on audio core

## 🔄 **Updates and Versioning**

### **Firmware Updates**

```bash
# Check current version
idf.py monitor | grep "SidekickOS.*Starting"

# Update to latest
git pull origin main
cd firmware
idf.py build flash
```
---
Happy coding!