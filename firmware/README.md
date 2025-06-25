# OpenSidekick ESP32S3 Firmware

This directory contains the ESP32S3 firmware for the OpenSidekick smart glasses project.

## 🚀 **Quick Start**

```bash
# Install ESP-IDF (if not already installed)
# Follow: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/

# Build and flash
idf.py build
idf.py flash monitor
```

## 📁 **Directory Structure**

```
firmware/
├── main/                   # Main application code
│   ├── main.c             # Primary firmware implementation
│   ├── CMakeLists.txt     # Main component build config
│   └── idf_component.yml  # Component dependencies
├── components/             # Custom components
│   └── posix_stub/        # POSIX compatibility layer
├── managed_components/     # ESP component dependencies
│   ├── espressif__esp32-camera/    # Camera driver
│   └── espressif__esp_h264/        # H.264 codec (future use)
├── CMakeLists.txt         # Project build configuration
├── dependencies.lock      # Dependency lock file
├── partitions.csv         # Flash partition table
├── sdkconfig*             # Build configuration files
└── README.md             # This file
```

## 🔧 **Features**

- **Camera**: OV2640 sensor with JPEG compression
- **Audio**: PDM microphone with G.711 μ-law encoding
- **BLE**: Optimized BLE 5.0 with 517-byte MTU
- **Performance**: 240MHz CPU, PSRAM optimization
- **Streaming**: Real-time image and audio streaming

## 📋 **Requirements**

- **Hardware**: XIAO ESP32S3 Sense
- **ESP-IDF**: Version 5.0 or later
- **Python**: Version 3.6 or later (for ESP-IDF)

## 🔨 **Build Instructions**

### **1. Setup ESP-IDF**
```bash
# Clone ESP-IDF
git clone -b v5.0 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh
source export.sh
```

### **2. Build Firmware**
```bash
cd opensidekick/firmware

# Configure (optional - defaults are optimized)
idf.py menuconfig

# Build
idf.py build

# Flash and monitor
idf.py flash monitor
```

## ⚙️ **Configuration**

Key configuration options:

- **Target**: ESP32-S3
- **CPU Frequency**: 240MHz
- **Flash Size**: 8MB
- **PSRAM**: Enabled (Octal, 80MHz)
- **BLE**: Enabled with Data Length Extension
- **Camera**: ESP32-Camera component

## 📊 **Performance**

- **Image Streaming**: 200-400 kbps
- **Audio Streaming**: 12.8 kbps
- **Frame Rate**: Up to 30 FPS
- **Latency**: 1-3 seconds per frame
- **Range**: 10-30 meters

## 🔧 **Customization**

See [firmware documentation](../docs/firmware.md) for detailed customization instructions.

## 🐛 **Troubleshooting**

### **Common Issues**

| Problem | Solution |
|---------|----------|
| Build fails | Ensure ESP-IDF v5.0+ is installed and sourced |
| Camera not working | Check hardware connections, try lower resolution |
| BLE not visible | Verify BLE is enabled in menuconfig |
| Flash errors | Check USB connection, try different cable |

### **Debug Output**

```bash
# Monitor with filtering
idf.py monitor --print_filter="*:I ESP32S3_CAMERA:D"

# Save debug output
idf.py monitor | tee debug.log
```

## 📞 **Support**

- **Documentation**: See [../docs/firmware.md](../docs/firmware.md)
- **Issues**: Open GitHub issue with firmware tag
- **ESP-IDF Help**: [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)

---

**Part of the OpenSidekick open-source smart glasses project 🤓** 