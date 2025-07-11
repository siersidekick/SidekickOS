# Hardware Guide - SidekickOS

## 🔧 **Required Hardware**

### **Primary Components**

#### **XIAO ESP32S3 Sense**
- **MCU**: ESP32-S3 dual-core processor @ 240MHz
- **Memory**: 8MB PSRAM, 8MB Flash
- **Camera**: OV2640 2MP camera sensor
- **Microphone**: PDM digital microphone
- **Connectivity**: WiFi 802.11 b/g/n, Bluetooth 5.0 BLE
- **Size**: 21×17.5mm (ultra-compact)
- **Power**: USB-C charging, 3.7V battery connector

**Purchase Links:**
- [Seeed Studio Official](https://www.seeedstudio.com/XIAO-ESP32S3-Sense-p-5639.html)
- [Amazon](https://amazon.com/dp/B0C69FFVHH)
- [Adafruit](https://www.adafruit.com/product/5426)

#### **Glasses Frame**
- Any standard eyeglasses frame

### **Optional Components**

#### **Battery Pack**
- **Capacity**: 100-500mAh recommended
- **Voltage**: 3.7V Li-Po battery
- **Size**: Small enough to fit in glasses arm or pocket

**Recommended Batteries:**
- Small form factor Li-Po batteries
- Capacity depends on use case and mounting solution

#### **Mounting Hardware**
- Mounting solutions currently in development
- Hardware assembly documentation coming soon

## 🔌 **Wiring and Connections**

**Note**: All pins are pre-configured in the firmware. No external wiring required for basic functionality.

## 🔋 **Power Management**

### **Power Consumption**
- **Idle**: ~50mA
- **Camera Active**: ~150-200mA
- **BLE Streaming**: ~200-250mA
- **Peak Usage**: ~300mA

### **Battery Life Estimates**
Battery life depends on:
- Camera resolution and quality settings
- Frame rate and streaming frequency
- Audio streaming usage (when implemented)
- BLE connection activity

Typical usage with optimized settings: 2-8 hours depending on battery capacity and usage patterns.

### **Power Optimization Tips**
- Use deep sleep between captures
- Reduce frame rate for longer battery life
- Lower camera resolution when possible
- Disable audio streaming when not needed

## 🏗️ **Mechanical Design**

### **Mounting Solutions**

Hardware mounting solutions and mechanical design are currently in development. This includes:

- Glasses frame integration options
- Battery pack mounting solutions
- Secure camera positioning systems
- Ergonomic design considerations

**Status:** Design and prototyping phase
**Expected:** Q1 2025 release of mounting hardware and assembly documentation

## 📐 **Dimensions and Clearances**

### **XIAO ESP32S3 Sense Dimensions**
- Length: 21mm
- Width: 17.5mm
- Height: 7.5mm (with camera)
- Weight: ~5g

### **Mounting Clearances**
- Camera field of view: 66° diagonal
- Minimum clearance from lens: 15mm
- Recommended mounting height: 5-10mm above frame
- USB-C access: 10mm clearance needed

## 🔧 **Assembly Instructions**

### **Current Status**

Assembly instructions and mounting solutions are currently in development.

**Available Now:**
- Firmware flashing and testing procedures
- Basic ESP32S3 functionality verification
- Software setup and configuration

**Coming Soon:**
- Complete assembly documentation
- Mounting hardware designs
- Step-by-step integration guides
- Safety and ergonomic considerations

For now, focus on firmware development and software testing using the ESP32S3 in a development setup.

## 🛡️ **Safety Considerations**

### **Electrical Safety**
- Use proper Li-Po battery handling procedures
- Avoid short circuits
- Monitor battery temperature during charging
- Use appropriate fuses if modifying power circuits

### **Mechanical Safety**
- Ensure secure mounting to prevent device falling
- Avoid sharp edges in 3D printed parts
- Test comfort during extended wear
- Consider impact resistance for active use

### **Privacy and Legal**
- Check local laws regarding recording devices
- Consider privacy implications of camera/microphone
- Add visible indicators when recording
- Respect others' privacy in public spaces

## 🔍 **Troubleshooting**

### **Common Hardware Issues**

| Problem | Possible Cause | Solution |
|---------|----------------|----------|
| Camera not working | Loose connection | Check camera ribbon cable |
| Poor image quality | Lens dirty/scratched | Clean lens, check for damage |
| Short battery life | High power consumption | Optimize firmware settings |
| BLE connection issues | Interference | Check for 2.4GHz interference |
| Charging problems | Faulty USB cable | Try different USB-C cable |

### **Hardware Testing**

```bash
# Flash test firmware
cd firmware
idf.py flash monitor

# Check hardware status in serial output
# Look for:
# - Camera initialization success
# - I2S/microphone setup
# - BLE advertising start
# - Memory allocation success
```

## 📞 **Hardware Support**

- **XIAO ESP32S3 Documentation**: [Seeed Studio Wiki](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
- **ESP32S3 Datasheet**: [Espressif Official](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- **Hardware Issues**: Open GitHub issue with hardware tag
- **Community**: Join discussions for hardware modifications and improvements

---

**Ready to build your own smart glasses? Let's get started! 🤓🔧** 