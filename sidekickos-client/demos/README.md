# SidekickOS Demos

This directory contains various demonstration scripts and examples for the SidekickOS platform.

## 📁 **Available Demos**

### **🤖 Gemini Live Demo** (`gemini_live/`)
A smart glasses demo that integrates the SidekickOS BLE camera with Google's Gemini Live API for real-time computer vision and audio interaction.

**Features:**
- Real-time image capture from SidekickOS camera
- Voice interaction with Gemini AI
- Audio responses describing what the AI sees
- True smart glasses experience

**Setup:** See `gemini_live/GEMINI_DEMO_README.md`

### **🐕 Dog Detection Demo** (`dog_detection/`)
Computer vision demo for detecting dogs in camera feeds.

### **📷 Basic Camera Usage** (`example_camera_usage.py`)
Basic examples showing how to use the SidekickOS camera for image capture and streaming.

## 🚀 **Quick Start**

1. **Install base dependencies:**
   ```bash
   pip install -r requirements.txt
   ```

2. **Choose a demo:**
   - For Gemini Live: `cd gemini_live && python sidekickos_gemini_demo.py`
   - For basic camera: `python example_camera_usage.py`

3. **Make sure your SidekickOS device is powered on and nearby**

## 📋 **Requirements**

- SidekickOS device (powered on and in range)
- Python 3.8+
- Additional requirements per demo (see individual README files)

## 💡 **Creating New Demos**

To add a new demo:

1. Create a new subdirectory: `mkdir your_demo_name/`
2. Add your demo files
3. Include a README.md with setup instructions
4. Update this index file

## 🆘 **Need Help?**

- Check individual demo README files for specific setup instructions
- Ensure your SidekickOS device is properly configured
- Verify all dependencies are installed

---

**Happy coding with SidekickOS!** 🤖👓 