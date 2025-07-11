# SidekickOS Python Library

A Python library for interfacing with the SidekickOS camera system. This library provides cross-platform support for Mac, Windows, and Linux using the `bleak` library.

## Features

- 📷 **Single Image Capture** - Capture high-quality JPEG images
- 🎥 **Video Streaming** - Continuous frame streaming with customizable frame rates
- ⚡ **High Performance** - Optimized for 517-byte MTU and 510-byte chunks
- 📊 **Performance Monitoring** - Real-time bandwidth and FPS tracking
- 🔧 **Camera Control** - Adjust quality, resolution, and frame intervals
- 🖼️ **PIL Integration** - Easy image processing with Python Imaging Library
- 🌍 **Cross-Platform** - Works on Mac, Windows, and Linux

## Installation

### Install as Library (Recommended)

Install the sidekickos library directly:

```bash
pip install -e .
```

Or for development:

```bash
git clone https://github.com/henrywarren/SidekickOS.git
cd SidekickOS/sidekickos-client
pip install -e .
```

### Manual Installation

Install dependencies manually:

```bash
pip install bleak pillow opencv-python ultralytics torch
```

### System Requirements

- **Mac**: Built-in Bluetooth LE support
- **Windows**: Windows 10/11 with Bluetooth LE support  
- **Linux**: BlueZ stack (usually pre-installed)

## Quick Start

### Basic Usage

```python
import asyncio
from sidekickos import ESP32Camera

async def main():
    camera = ESP32Camera()
    
    # Connect to camera
    if await camera.connect():
        # Capture a single image
        image = await camera.capture_image()
        if image:
            image.save("my_photo.jpg")
            print(f"Image saved: {image.size} bytes")
        
        await camera.disconnect()

asyncio.run(main())
```

### Streaming Example

```python
import asyncio
from sidekickos import ESP32Camera

def frame_callback(frame):
    print(f"Frame {frame.frame_number}: {frame.size} bytes")
    frame.save(f"frame_{frame.frame_number:04d}.jpg")

async def stream_example():
    camera = ESP32Camera()
    
    if await camera.connect():
        # Start streaming at 2 FPS
        await camera.start_streaming(
            callback=frame_callback,
            interval=0.5,  # 0.5 seconds = 2 FPS
            quality=25
        )
        
        # Stream for 10 seconds
        await asyncio.sleep(10)
        await camera.stop_streaming()
        await camera.disconnect()

asyncio.run(stream_example())
```

## Running the Examples

The project includes a comprehensive example script:

```bash
python example_camera_usage.py
```

This provides:
1. **Single Image Capture** - High-quality photo capture
2. **Video Streaming** - Continuous frame capture with auto-save
3. **Performance Testing** - Benchmark different quality/speed settings
4. **Interactive Mode** - Real-time camera control

## API Reference

### ESP32Camera Class

#### Connection Methods
- `connect(device_address=None, timeout=10.0)` - Connect to camera
- `disconnect()` - Disconnect from camera
- `scan_for_device(timeout=10.0)` - Scan for ESP32S3 camera

#### Image Capture
- `capture_image(timeout=10.0)` - Capture single image
- `start_streaming(callback, interval=0.5, quality=25)` - Start streaming
- `stop_streaming()` - Stop streaming

#### Camera Control
- `set_quality(quality)` - Set JPEG quality (4-63, lower = better)
- `set_resolution(size_code)` - Set camera resolution
- `set_interval(interval)` - Set frame interval in seconds
- `send_command(command)` - Send raw command to ESP32

#### Performance
- `get_performance_stats()` - Get bandwidth/FPS statistics

### ImageFrame Class

#### Properties
- `data` - Raw JPEG image data (bytes)
- `size` - Image size in bytes
- `timestamp` - Capture timestamp
- `frame_number` - Sequential frame number
- `completion_rate` - Percentage of chunks received

#### Methods
- `save(filename)` - Save image to file
- `to_pil_image()` - Convert to PIL Image for processing

## Camera Settings

### Resolution Codes
- `1` - QQVGA (160×120) - Fastest
- `5` - QVGA (320×240) - **Recommended**
- `6` - CIF (400×296)
- `7` - HVGA (480×320)
- `8` - VGA (640×480) - Highest quality

### Quality Settings
- `4-10` - Highest quality (larger files)
- `15-25` - **Recommended range**
- `30-50` - Lower quality (smaller files)
- `51-63` - Lowest quality

### Frame Intervals
- `0.1s` - 10 FPS (very fast)
- `0.5s` - 2 FPS (**recommended**)
- `1.0s` - 1 FPS (conservative)
- `2.0s+` - Slow capture

## Performance Optimization

### For Maximum Speed
```python
await camera.set_quality(30)      # Lower quality
await camera.set_resolution(1)    # Smallest resolution
await camera.set_interval(0.2)    # 5 FPS
```

### For Maximum Quality
```python
await camera.set_quality(10)      # High quality
await camera.set_resolution(8)    # VGA resolution
await camera.set_interval(2.0)    # Slower capture
```

### For Balanced Performance
```python
await camera.set_quality(25)      # Balanced quality
await camera.set_resolution(5)    # QVGA resolution
await camera.set_interval(0.5)    # 2 FPS
```

## Troubleshooting

### Connection Issues
1. **Camera not found**: Ensure ESP32S3 is powered on and advertising
2. **Connection timeout**: Try increasing the timeout parameter
3. **Service not found**: Verify ESP32S3 firmware is running correctly

### Performance Issues
1. **Slow transfer**: Check BLE signal strength and reduce distance
2. **Dropped frames**: Increase frame interval or reduce quality
3. **Memory errors**: Reduce resolution or implement frame buffering

### Platform-Specific Issues

#### Mac
- Grant Bluetooth permissions to Terminal/Python
- Ensure Bluetooth is enabled in System Preferences

#### Windows
- Install latest Bluetooth drivers
- Enable Bluetooth in Windows Settings
- May require running as Administrator

#### Linux
- Ensure BlueZ is installed: `sudo apt install bluez`
- Add user to bluetooth group: `sudo usermod -a -G bluetooth $USER`

## Performance Expectations

With optimized settings (QVGA, quality 25, 2 FPS):
- **Bandwidth**: 200-400 kbps
- **Frame Size**: 8-15 KB per frame
- **Latency**: 1-3 seconds per frame
- **Range**: 10-30 meters (depending on environment)

## Integration Examples

### Save to Different Formats
```python
# Save as different formats using PIL
pil_image = frame.to_pil_image()
pil_image.save("image.png", "PNG")
pil_image.save("image.bmp", "BMP")
```

### Image Processing
```python
from PIL import ImageEnhance

# Enhance captured image
pil_image = frame.to_pil_image()
enhancer = ImageEnhance.Brightness(pil_image)
bright_image = enhancer.enhance(1.5)
bright_image.save("enhanced.jpg")
```

### Real-time Display
```python
import matplotlib.pyplot as plt

def display_callback(frame):
    pil_image = frame.to_pil_image()
    plt.imshow(pil_image)
    plt.title(f"Frame {frame.frame_number}")
    plt.pause(0.1)
```

## License

This project is part of the ESP32S3 BLE Camera system. See the main project documentation for license details.

## Support

For issues and questions:
1. Check the troubleshooting section above
2. Verify ESP32S3 firmware is working with the HTML client
3. Test BLE connectivity with other devices
4. Check Python and dependency versions 