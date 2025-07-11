# SidekickOS Python Library

This directory contains the SidekickOS Python library for ESP32S3 BLE camera interface.

## Quick Start

```bash
# Install the library
pip install -e .

# Run a demo
python demos/example_camera_usage.py
```

## 📚 Documentation

For complete documentation, please see:

- **[Python Library Guide](../docs/python-library.md)** - Complete API documentation
- **[Demo Scripts Guide](../docs/demos.md)** - Guide to all demo applications
- **[Main Project README](../README.md)** - Project overview and getting started

## Structure

```
sidekickos-client/
├── sidekickos/           # Main Python library
│   └── __init__.py        # ESP32Camera, ImageFrame classes
├── demos/                 # Example scripts
│   ├── example_camera_usage.py
│   └── dog_detection/
├── setup.py               # Installation configuration
└── requirements.txt       # Dependencies
```

## Quick Install & Test

```bash
pip install -e .
python -c "from sidekickos import ESP32Camera; print('✅ Installation successful!')"
``` 