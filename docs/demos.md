# SidekickOS Demo Scripts

This directory contains demonstration scripts for the SidekickOS ESP32 camera system.

## Prerequisites

Install the sidekickos library:

```bash
cd ..
pip install -e .
```

Or install dependencies manually:

```bash
cd ..
pip install -r requirements.txt
```

## Demo Scripts

### Camera Examples

**example_camera_usage.py**
Basic camera usage examples: single image capture, video streaming, performance testing, interactive mode.

```bash
python example_camera_usage.py
```

**sidekickos-web-client.html**
Web-based camera interface. Open in any web browser and follow instructions to connect.

### Dog Detection

Dog detection demos are in the `dog_detection/` folder:

**dog_detection/simple_dog_detector.py**
Streamlined dog detection. Saves clean images to "cute dogs" folder.

```bash
cd dog_detection
python simple_dog_detector.py
```

**dog_detection/dog_detector_demo.py**
Advanced dog detection with performance monitoring and statistics.

```bash
cd dog_detection
python dog_detector_demo.py
```

### 🎥 Video Demo

**Dog Detection in Action**
Watch the SidekickOS dog detection demo in action:

📹 **[Dog Detection Demo Video](https://drive.google.com/file/d/1LnD9ZVXxRB4CjpYO92vVTl6aJtVk8wdE/view?usp=sharing)**

This video demonstrates:
- Real-time camera streaming from ESP32S3
- Live dog detection using YOLOv5
- Clean image saving without bounding boxes
- Performance monitoring and statistics
- Bluetooth LE connectivity in action

> **Note**: The video shows the `simple_dog_detector.py` script detecting dogs and automatically saving photos to the "cute dogs" folder.

## Running Demos

1. Connect your ESP32 camera and make sure it's powered on
2. Navigate to the appropriate directory
3. Run the script

## Troubleshooting

- Import errors: Make sure you're running from the correct directory
- Camera connection issues: Ensure ESP32 is powered and Bluetooth is enabled
- Missing dependencies: Run `pip install -r ../requirements.txt` 