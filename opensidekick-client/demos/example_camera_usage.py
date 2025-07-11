#!/usr/bin/env python3
"""
ESP32S3 BLE Camera Usage Examples

This script demonstrates how to use the ESP32Camera module for:
1. Single image capture
2. Continuous streaming
3. Performance monitoring

Requirements:
    pip install bleak pillow

Usage:
    python example_camera_usage.py
"""

import asyncio
import os
import time
import sys
from datetime import datetime

# Add parent directory to path for imports
sys.path.append('..')
from opensidekick import ESP32Camera, ImageFrame


async def capture_single_image():
    """Example: Capture a single image"""
    print("🔍 Single Image Capture Example")
    print("=" * 40)
    
    camera = ESP32Camera()
    
    try:
        # Connect to camera
        print("Connecting to OpenSidekick camera...")
        if not await camera.connect():
            print("❌ Failed to connect to camera")
            return
        
        # Set high quality for single capture
        await camera.set_quality(15)  # Higher quality (lower number = better quality)
        await camera.set_resolution(5)  # QVGA (320x240)
        
        # Capture image
        print("📷 Capturing image...")
        image = await camera.capture_image(timeout=15.0)
        
        if image:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"capture_{timestamp}.jpg"
            image.save(filename)
            
            print(f"✅ Image captured successfully!")
            print(f"   Size: {image.size:,} bytes")
            print(f"   Completion: {image.completion_rate:.1f}%")
            print(f"   Saved as: {filename}")
            
            # Convert to PIL for analysis
            pil_image = image.to_pil_image()
            print(f"   Dimensions: {pil_image.size[0]}x{pil_image.size[1]}")
            print(f"   Format: {pil_image.format}")
        else:
            print("❌ Failed to capture image")
    
    finally:
        await camera.disconnect()


async def stream_video():
    """Example: Stream video frames"""
    print("\n🎥 Video Streaming Example")
    print("=" * 40)
    
    camera = ESP32Camera()
    frame_count = 0
    start_time = time.time()
    
    # Create output directory
    output_dir = f"stream_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    os.makedirs(output_dir, exist_ok=True)
    print(f"📁 Saving frames to: {output_dir}/")
    
    def frame_callback(frame: ImageFrame):
        nonlocal frame_count
        frame_count += 1
        
        # Save every frame
        filename = os.path.join(output_dir, f"frame_{frame.frame_number:04d}.jpg")
        frame.save(filename)
        
        # Progress update
        elapsed = time.time() - start_time
        fps = frame_count / elapsed if elapsed > 0 else 0
        
        print(f"📸 Frame {frame.frame_number}: {frame.size:,} bytes "
              f"({frame.completion_rate:.1f}%) - {fps:.1f} FPS")
    
    try:
        # Connect to camera
        print("Connecting to OpenSidekick camera...")
        if not await camera.connect():
            print("❌ Failed to connect to camera")
            return
        
        # Configure for streaming
        await camera.set_quality(25)  # Balanced quality for streaming
        await camera.set_resolution(5)  # QVGA (320x240)
        
        # Start streaming
        print("📹 Starting video stream for 15 seconds...")
        print("   Press Ctrl+C to stop early")
        
        await camera.start_streaming(
            callback=frame_callback,
            interval=0.5,  # 2 FPS
            quality=25
        )
        
        # Stream for 15 seconds
        try:
            await asyncio.sleep(15)
        except KeyboardInterrupt:
            print("\n⏹️ Stopping stream (user interrupted)")
        
        await camera.stop_streaming()
        
        # Show final stats
        stats = camera.get_performance_stats()
        print(f"\n📊 Streaming Complete!")
        print(f"   Frames captured: {stats['frames_received']}")
        print(f"   Total data: {stats['bytes_received']:,} bytes")
        print(f"   Average FPS: {stats.get('avg_fps', 0):.2f}")
        print(f"   Average bandwidth: {stats.get('avg_kbps', 0):.1f} kbps")
        print(f"   Frames saved to: {output_dir}/")
    
    finally:
        await camera.disconnect()


async def performance_test():
    """Example: Performance testing with different settings"""
    print("\n⚡ Performance Test Example")
    print("=" * 40)
    
    camera = ESP32Camera()
    
    try:
        # Connect to camera
        print("Connecting to OpenSidekick camera...")
        if not await camera.connect():
            print("❌ Failed to connect to camera")
            return
        
        # Test different frame rates
        test_configs = [
            {"interval": 1.0, "quality": 30, "name": "1 FPS, Low Quality"},
            {"interval": 0.5, "quality": 25, "name": "2 FPS, Medium Quality"},
            {"interval": 0.2, "quality": 20, "name": "5 FPS, High Quality"},
        ]
        
        for config in test_configs:
            print(f"\n🧪 Testing: {config['name']}")
            
            frame_count = 0
            def test_callback(frame: ImageFrame):
                nonlocal frame_count
                frame_count += 1
                if frame_count <= 3:  # Show first 3 frames
                    print(f"   Frame {frame_count}: {frame.size:,} bytes")
            
            # Reset stats
            camera.performance_stats = {
                'frames_received': 0,
                'bytes_received': 0,
                'start_time': time.time(),
                'last_frame_time': None
            }
            
            # Test for 6 seconds
            await camera.start_streaming(
                callback=test_callback,
                interval=config["interval"],
                quality=config["quality"]
            )
            
            await asyncio.sleep(6)
            await camera.stop_streaming()
            
            # Show results
            stats = camera.get_performance_stats()
            print(f"   📊 Results: {stats['frames_received']} frames, "
                  f"{stats.get('avg_fps', 0):.1f} FPS, "
                  f"{stats.get('avg_kbps', 0):.1f} kbps")
    
    finally:
        await camera.disconnect()


async def interactive_mode():
    """Interactive mode for testing camera features"""
    print("\n🎮 Interactive Mode")
    print("=" * 40)
    
    camera = ESP32Camera()
    
    try:
        # Connect to camera
        print("Connecting to OpenSidekick camera...")
        if not await camera.connect():
            print("❌ Failed to connect to camera")
            return
        
        print("\n🎛️ Camera connected! Available commands:")
        print("  'c' - Capture single image")
        print("  's' - Start streaming (10 seconds)")
        print("  'q' - Set quality (4-63)")
        print("  'r' - Set resolution (1-8)")
        print("  'p' - Show performance stats")
        print("  'x' - Exit")
        
        while True:
            try:
                cmd = input("\n> ").strip().lower()
                
                if cmd == 'c':
                    print("📷 Capturing...")
                    image = await camera.capture_image()
                    if image:
                        filename = f"interactive_{int(time.time())}.jpg"
                        image.save(filename)
                        print(f"✅ Saved as {filename} ({image.size:,} bytes)")
                
                elif cmd == 's':
                    print("📹 Streaming for 10 seconds...")
                    
                    def stream_callback(frame: ImageFrame):
                        print(f"Frame {frame.frame_number}: {frame.size:,} bytes")
                    
                    await camera.start_streaming(stream_callback, interval=0.5)
                    await asyncio.sleep(10)
                    await camera.stop_streaming()
                    print("⏹️ Streaming stopped")
                
                elif cmd == 'q':
                    quality = int(input("Enter quality (4-63, lower=better): "))
                    await camera.set_quality(quality)
                    print(f"Quality set to {quality}")
                
                elif cmd == 'r':
                    print("Resolutions: 1=QQVGA(160x120), 5=QVGA(320x240), 8=VGA(640x480)")
                    resolution = int(input("Enter resolution code: "))
                    await camera.set_resolution(resolution)
                    print(f"Resolution set to {resolution}")
                
                elif cmd == 'p':
                    stats = camera.get_performance_stats()
                    print(f"📊 Performance Stats:")
                    print(f"   Frames: {stats['frames_received']}")
                    print(f"   Data: {stats['bytes_received']:,} bytes")
                    print(f"   FPS: {stats.get('avg_fps', 0):.2f}")
                    print(f"   Bandwidth: {stats.get('avg_kbps', 0):.1f} kbps")
                
                elif cmd == 'x':
                    break
                
                else:
                    print("❓ Unknown command")
            
            except KeyboardInterrupt:
                break
            except Exception as e:
                print(f"❌ Error: {e}")
    
    finally:
        await camera.disconnect()


async def main():
    """Main function with menu"""
    print("🤖 OpenSidekick BLE Camera Python Interface")
    print("=" * 50)
    print("Make sure your OpenSidekick camera is powered on and advertising!")
    print()
    
    while True:
        print("Select an example:")
        print("1. Single Image Capture")
        print("2. Video Streaming")
        print("3. Performance Test")
        print("4. Interactive Mode")
        print("5. Exit")
        
        try:
            choice = input("\nEnter choice (1-5): ").strip()
            
            if choice == '1':
                await capture_single_image()
            elif choice == '2':
                await stream_video()
            elif choice == '3':
                await performance_test()
            elif choice == '4':
                await interactive_mode()
            elif choice == '5':
                print("👋 Goodbye!")
                break
            else:
                print("❓ Invalid choice")
        
        except KeyboardInterrupt:
            print("\n👋 Goodbye!")
            break
        except Exception as e:
            print(f"❌ Error: {e}")


if __name__ == "__main__":
    asyncio.run(main()) 