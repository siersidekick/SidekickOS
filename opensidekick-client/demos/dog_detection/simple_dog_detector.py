#!/usr/bin/env python3
"""
Simple Dog Detection - No GUI Complications

Just detects dogs and saves frames with bounding boxes.
"""

import asyncio
import cv2
import numpy as np
import sys
from datetime import datetime
from pathlib import Path

# Add parent directories to path for imports
sys.path.append('../..')
from opensidekick import ESP32Camera
from dog_detector_demo import DogDetector

class SimpleDogDetector:
    def __init__(self):
        self.camera = ESP32Camera()
        self.detector = DogDetector(0.4)
        self.frame_count = 0
        self.save_dir = Path("cute dogs")
        self.save_dir.mkdir(exist_ok=True)
        
    async def start(self):
        print("🐕 Simple Dog Detection Starting...")
        
        if not await self.camera.connect():
            print("❌ Camera connection failed")
            return
            
        # Fast settings: VGA res, lower quality for speed
        await self.camera.set_resolution(7)  # VGA 480x320
        await self.camera.set_quality(25)    # Lower quality for speed
        
        await self.camera.start_streaming(self.process_frame, interval=0.2)  # 5 FPS
        
        try:
            while True:
                await asyncio.sleep(1)
        except KeyboardInterrupt:
            print("\n⏹️ Stopping...")
            
        await self.camera.stop_streaming()
        await self.camera.disconnect()
        cv2.destroyAllWindows()
        
    def process_frame(self, frame):
        self.frame_count += 1
        
        # Convert to OpenCV
        pil_image = frame.to_pil_image()
        cv_image = cv2.cvtColor(np.array(pil_image), cv2.COLOR_RGB2BGR)
        
        # Show frame immediately (no waiting for detection)
        cv2.imshow('Dog Detection', cv_image)
        cv2.waitKey(1)
        
        # Start async detection without blocking
        asyncio.create_task(self.detect_async(cv_image.copy(), self.frame_count))
    
    async def detect_async(self, cv_image, frame_num):
        # Detect dogs async
        detections = self.detector.detect_dogs(cv_image)
        
        # Debug: show ALL detections, not just dogs
        if frame_num % 10 == 0:  # Print debug every 10 frames
            results = self.detector.model(cv_image)
            for result in results:
                if result.boxes is not None:
                    for box in result.boxes:
                        class_id = int(box.cls.item())
                        confidence = float(box.conf.item())
                        class_name = self.detector.class_names[class_id] if class_id < len(self.detector.class_names) else "unknown"
                        print(f"  Detected: {class_name} (ID:{class_id}) confidence:{confidence:.2f}")
        
        if detections:
            # Save original image without bounding boxes
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"dog_frame_{frame_num}_{timestamp}.jpg"
            save_path = self.save_dir / filename
            cv2.imwrite(str(save_path), cv_image)
            
            # Draw bounding boxes on a copy for display
            display_image = cv_image.copy()
            for detection in detections:
                bbox = detection['bbox']
                confidence = detection['confidence']
                cv2.rectangle(display_image, (bbox[0], bbox[1]), (bbox[2], bbox[3]), (0, 255, 0), 2)
                cv2.putText(display_image, f"Dog {confidence:.2f}", (bbox[0], bbox[1]-10), 
                           cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)
            
            # Show frame with detections
            cv2.imshow('Dog Detection', display_image)
            cv2.waitKey(1)
            
            print(f"🐕 FOUND DOG! Frame {frame_num} - saved to {save_path}")

if __name__ == "__main__":
    detector = SimpleDogDetector()
    asyncio.run(detector.start()) 