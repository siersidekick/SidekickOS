#!/usr/bin/env python3
"""
🐕 SidekickOS Dog Detection Demo

This demo uses the ESP32 BLE camera to automatically detect and capture photos of dogs.
It combines real-time computer vision with the SidekickOS camera system.

Features:
- Real-time dog detection using YOLOv5
- Automatic photo capture when dogs are detected
- Confidence scoring and filtering
- Performance monitoring
- Photo organization and timestamps

Requirements:
    pip install bleak pillow opencv-python torch torchvision ultralytics numpy

Usage:
    python dog_detector_demo.py
"""

import asyncio
import os
import time
import cv2
import numpy as np
import sys
from datetime import datetime
from pathlib import Path
from typing import Optional, List, Dict, Tuple
import logging

# Add parent directories to path for imports
sys.path.append('../..')

# Import our ESP32 camera module
from sidekickos import ESP32Camera, ImageFrame

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class DogDetector:
    """Computer vision dog detection system"""
    
    def __init__(self, confidence_threshold: float = 0.5):
        """
        Initialize dog detector
        
        Args:
            confidence_threshold: Minimum confidence score for detection (0.0-1.0)
        """
        self.confidence_threshold = confidence_threshold
        self.model = None
        self.class_names = []
        self.dog_class_ids = []
        self.stats = {
            'total_frames': 0,
            'dogs_detected': 0,
            'photos_captured': 0,
            'false_positives': 0,
            'avg_confidence': 0.0
        }
        
        # Setup model
        self._setup_model()
    
    def _setup_model(self):
        """Setup YOLO model for object detection"""
        try:
            # Try to import YOLOv5 (ultralytics)
            from ultralytics import YOLO
            
            print("🧠 Loading YOLOv5 model...")
            self.model = YOLO('yolov5m.pt')  # Medium model for better accuracy
            
            # COCO dataset class names
            self.class_names = [
                'person', 'bicycle', 'car', 'motorcycle', 'airplane', 'bus', 'train', 'truck', 'boat',
                'traffic light', 'fire hydrant', 'stop sign', 'parking meter', 'bench', 'bird', 'cat',
                'dog', 'horse', 'sheep', 'cow', 'elephant', 'bear', 'zebra', 'giraffe', 'backpack',
                'umbrella', 'handbag', 'tie', 'suitcase', 'frisbee', 'skis', 'snowboard', 'sports ball',
                'kite', 'baseball bat', 'baseball glove', 'skateboard', 'surfboard', 'tennis racket',
                'bottle', 'wine glass', 'cup', 'fork', 'knife', 'spoon', 'bowl', 'banana', 'apple',
                'sandwich', 'orange', 'broccoli', 'carrot', 'hot dog', 'pizza', 'donut', 'cake',
                'chair', 'couch', 'potted plant', 'bed', 'dining table', 'toilet', 'tv', 'laptop',
                'mouse', 'remote', 'keyboard', 'cell phone', 'microwave', 'oven', 'toaster', 'sink',
                'refrigerator', 'book', 'clock', 'vase', 'scissors', 'teddy bear', 'hair drier', 'toothbrush'
            ]
            
            # Find dog class index
            self.dog_class_ids = [i for i, name in enumerate(self.class_names) if name == 'dog']
            print(f"✅ YOLOv5 model loaded successfully!")
            print(f"🐕 Dog class ID: {self.dog_class_ids}")
            
        except ImportError:
            print("⚠️ YOLOv5 not available, trying OpenCV DNN...")
            self._setup_opencv_model()
    
    def _setup_opencv_model(self):
        """Fallback to OpenCV DNN model"""
        try:
            # Download YOLOv4 weights if not present
            model_path = Path("yolov4.weights")
            config_path = Path("yolov4.cfg")
            
            if not model_path.exists() or not config_path.exists():
                print("📥 Downloading YOLOv4 model files...")
                # In a real implementation, you'd download these files
                # For now, we'll use a placeholder
                raise FileNotFoundError("YOLOv4 model files not found")
            
            # Load OpenCV DNN model
            self.model = cv2.dnn.readNetFromDarknet(str(config_path), str(model_path))
            print("✅ OpenCV DNN model loaded successfully!")
            
        except Exception as e:
            print(f"❌ Failed to load computer vision model: {e}")
            print("📋 Please install required dependencies:")
            print("   pip install ultralytics torch torchvision")
            self.model = None
    
    def detect_dogs(self, image: np.ndarray) -> List[Dict]:
        """
        Detect dogs in an image
        
        Args:
            image: OpenCV image (BGR format)
            
        Returns:
            List of detection dictionaries with bbox, confidence, etc.
        """
        if self.model is None:
            return []
        
        self.stats['total_frames'] += 1
        
        try:
            # Run YOLOv5 inference
            results = self.model(image)
            
            detections = []
            confidences = []
            
            # Process results
            for result in results:
                boxes = result.boxes
                if boxes is not None:
                    for box in boxes:
                        # Get class ID and confidence
                        class_id = int(box.cls.item())
                        confidence = float(box.conf.item())
                        
                        # Check if it's a dog and meets confidence threshold
                        if class_id in self.dog_class_ids and confidence >= self.confidence_threshold:
                            # Get bounding box coordinates
                            x1, y1, x2, y2 = box.xyxy[0].tolist()
                            
                            detection = {
                                'bbox': [int(x1), int(y1), int(x2), int(y2)],
                                'confidence': confidence,
                                'class_name': 'dog',
                                'class_id': class_id
                            }
                            detections.append(detection)
                            confidences.append(confidence)
            
            # Update stats
            if detections:
                self.stats['dogs_detected'] += 1
                self.stats['avg_confidence'] = np.mean(confidences)
                logger.info(f"🐕 Found {len(detections)} dog(s) with avg confidence {self.stats['avg_confidence']:.2f}")
            
            return detections
            
        except Exception as e:
            logger.error(f"Error in dog detection: {e}")
            return []
    
    def draw_detections(self, image: np.ndarray, detections: List[Dict]) -> np.ndarray:
        """
        Draw detection bounding boxes on image
        
        Args:
            image: OpenCV image
            detections: List of detection dictionaries
            
        Returns:
            Image with bounding boxes drawn
        """
        result_image = image.copy()
        
        for detection in detections:
            bbox = detection['bbox']
            confidence = detection['confidence']
            
            # Draw bounding box
            cv2.rectangle(result_image, (bbox[0], bbox[1]), (bbox[2], bbox[3]), (0, 255, 0), 2)
            
            # Draw label
            label = f"Dog {confidence:.2f}"
            cv2.putText(result_image, label, (bbox[0], bbox[1] - 10), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)
        
        return result_image
    
    def get_stats(self) -> Dict:
        """Get detection statistics"""
        return self.stats.copy()


class SmartDogCamera:
    """Smart camera system that automatically captures photos of dogs"""
    
    def __init__(self, confidence_threshold: float = 0.6):
        """
        Initialize smart dog camera
        
        Args:
            confidence_threshold: Minimum confidence for dog detection
        """
        self.camera = ESP32Camera()
        self.detector = DogDetector(confidence_threshold)
        self.is_running = False
        self.last_detection_time = 0
        self.detection_cooldown = 2.0  # Seconds between captures
        
        # Create output directories
        self.output_dir = Path(f"dog_photos_{datetime.now().strftime('%Y%m%d_%H%M%S')}")
        self.output_dir.mkdir(exist_ok=True)
        
        self.debug_dir = self.output_dir / "debug"
        self.debug_dir.mkdir(exist_ok=True)
        
        print(f"📁 Photos will be saved to: {self.output_dir}")
    
    async def start_detection(self, stream_duration: float = 60.0):
        """
        Start the dog detection and capture system
        
        Args:
            stream_duration: How long to run detection (seconds)
        """
        print("🚀 Starting Smart Dog Camera System")
        print("=" * 50)
        
        try:
            # Connect to camera
            print("📡 Connecting to SidekickOS camera...")
            if not await self.camera.connect():
                print("❌ Failed to connect to camera")
                return
            
            # Configure camera for optimal detection
            await self.camera.set_quality(63)  # Good quality for detection
            await self.camera.set_resolution(5)  # VGA for better detail
            
            print("🔍 Starting dog detection stream...")
            print(f"⏱️  Will run for {stream_duration} seconds")
            print("🐕 Watching for dogs...")
            
            self.is_running = True
            
            # Start streaming with our detection callback
            await self.camera.start_streaming(
                callback=self._process_frame,
                interval=1.0,  # 1 FPS for detection
                quality=15
            )
            
            # Run detection for specified duration
            start_time = time.time()
            try:
                while time.time() - start_time < stream_duration and self.is_running:
                    await asyncio.sleep(1.0)
                    
                    # Show periodic stats
                    if int(time.time() - start_time) % 10 == 0:
                        self._print_stats()
                        
            except KeyboardInterrupt:
                print("\n⏹️ Stopping detection (user interrupted)")
            
            # Stop streaming
            await self.camera.stop_streaming()
            
            # Final stats
            self._print_final_stats()
            
        finally:
            await self.camera.disconnect()
            self.is_running = False
    
    def _process_frame(self, frame: ImageFrame):
        """Process each camera frame for dog detection"""
        try:
            # Convert frame to OpenCV format
            pil_image = frame.to_pil_image()
            cv_image = cv2.cvtColor(np.array(pil_image), cv2.COLOR_RGB2BGR)
            
            # Detect dogs
            detections = self.detector.detect_dogs(cv_image)
            
            # If dogs detected and cooldown period has passed
            current_time = time.time()
            if detections and (current_time - self.last_detection_time) > self.detection_cooldown:
                self._capture_dog_photo(frame, cv_image, detections)
                self.last_detection_time = current_time
            
            # Save debug frame occasionally
            if frame.frame_number % 10 == 0:
                debug_image = self.detector.draw_detections(cv_image, detections)
                debug_path = self.debug_dir / f"debug_frame_{frame.frame_number:04d}.jpg"
                cv2.imwrite(str(debug_path), debug_image)
            
        except Exception as e:
            logger.error(f"Error processing frame {frame.frame_number}: {e}")
    
    def _capture_dog_photo(self, frame: ImageFrame, cv_image: np.ndarray, detections: List[Dict]):
        """Capture and save a photo when dogs are detected"""
        try:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:-3]
            
            # Save original photo
            original_path = self.output_dir / f"dog_photo_{timestamp}.jpg"
            frame.save(str(original_path))
            
            # Save annotated photo
            annotated_image = self.detector.draw_detections(cv_image, detections)
            annotated_path = self.output_dir / f"dog_detected_{timestamp}.jpg"
            cv2.imwrite(str(annotated_path), annotated_image)
            
            # Update stats
            self.detector.stats['photos_captured'] += 1
            
            # Log the capture
            total_confidence = sum(d['confidence'] for d in detections)
            avg_confidence = total_confidence / len(detections)
            
            print(f"📸 DOG DETECTED! Captured photo #{self.detector.stats['photos_captured']}")
            print(f"   🎯 Confidence: {avg_confidence:.2f}")
            print(f"   📷 Count: {len(detections)} dog(s)")
            print(f"   💾 Saved: {original_path.name}")
            print(f"   🔍 Annotated: {annotated_path.name}")
            
        except Exception as e:
            logger.error(f"Error capturing dog photo: {e}")
    
    def _print_stats(self):
        """Print current detection statistics"""
        stats = self.detector.get_stats()
        camera_stats = self.camera.get_performance_stats()
        
        print(f"\n📊 Current Stats:")
        print(f"   📹 Frames processed: {stats['total_frames']}")
        print(f"   🐕 Dogs detected: {stats['dogs_detected']}")
        print(f"   📸 Photos captured: {stats['photos_captured']}")
        print(f"   🎯 Avg confidence: {stats['avg_confidence']:.2f}")
        print(f"   📡 Camera FPS: {camera_stats.get('avg_fps', 0):.1f}")
        print(f"   💾 Total photos in: {self.output_dir}")
    
    def _print_final_stats(self):
        """Print final detection statistics"""
        stats = self.detector.get_stats()
        camera_stats = self.camera.get_performance_stats()
        
        print("\n🎉 Dog Detection Session Complete!")
        print("=" * 50)
        print(f"📊 Final Statistics:")
        print(f"   📹 Total frames: {stats['total_frames']}")
        print(f"   🐕 Dogs detected: {stats['dogs_detected']}")
        print(f"   📸 Photos captured: {stats['photos_captured']}")
        print(f"   🎯 Detection rate: {(stats['dogs_detected']/stats['total_frames']*100):.1f}%")
        print(f"   📡 Average FPS: {camera_stats.get('avg_fps', 0):.1f}")
        print(f"   💾 Photos saved to: {self.output_dir}")
        print(f"   🔍 Debug frames in: {self.debug_dir}")
        
        if stats['photos_captured'] > 0:
            print(f"\n🐕 Successfully captured {stats['photos_captured']} dog photos!")
        else:
            print(f"\n😔 No dogs detected during this session")


async def main():
    """Main demo function"""
    print("🐕 SidekickOS Dog Detection Demo")
    print("=" * 40)
    print("This demo will automatically detect and capture photos of dogs")
    print("using your SidekickOS camera and computer vision.")
    print()
    
    # Configuration
    confidence_threshold = 0.6  # Adjust based on your needs
    stream_duration = 300.0     # 5 minutes of detection
    
    print(f"⚙️  Configuration:")
    print(f"   🎯 Confidence threshold: {confidence_threshold}")
    print(f"   ⏱️  Stream duration: {stream_duration} seconds")
    print(f"   📷 Camera quality: High (15)")
    print(f"   📹 Frame rate: 1 FPS")
    print()
    
    # Create and start smart camera
    smart_camera = SmartDogCamera(confidence_threshold)
    
    try:
        await smart_camera.start_detection(stream_duration)
    except KeyboardInterrupt:
        print("\n👋 Demo stopped by user")
    except Exception as e:
        print(f"\n❌ Demo error: {e}")
    
    print("\n🎬 Demo complete!")


if __name__ == "__main__":
    asyncio.run(main()) 