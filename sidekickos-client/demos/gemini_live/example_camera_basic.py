#!/usr/bin/env python3
"""
Basic SidekickOS Camera Example

A simple example showing how to use the SidekickOS BLE camera for basic image capture.
This demonstrates the core functionality without any AI integration.
"""

import asyncio
import logging
import sys
import os

# Add parent directory to path for sidekickos import
sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))
from sidekickos import ESP32Camera

# Configure logging
logging.basicConfig(level=logging.INFO)

async def basic_camera_example():
    """Basic camera capture example"""
    print("📷 SidekickOS Camera Basic Example")
    print("=" * 40)
    
    # Create camera instance
    camera = ESP32Camera()
    
    try:
        # Connect to camera
        print("🔍 Connecting to SidekickOS camera...")
        if not await camera.connect():
            print("❌ Failed to connect to camera")
            return
        
        print("✅ Connected successfully!")
        
        # Configure camera settings
        print("⚙️  Configuring camera...")
        await camera.set_quality(25)  # Good quality
        await camera.set_resolution(5)  # QVGA (320x240)
        
        # Capture multiple images
        for i in range(5):
            print(f"\n📸 Capturing image {i+1}/5...")
            
            # Capture single image
            image = await camera.capture_image()
            
            if image:
                print(f"✅ Image captured successfully!")
                print(f"   Size: {image.size} bytes")
                print(f"   Chunks: {image.chunks_received}/{image.chunks_expected}")
                print(f"   Completion: {image.completion_rate:.1f}%")
                print(f"   Timestamp: {image.timestamp}")
                
                # Save image
                filename = f"captured_image_{i+1}.jpg"
                image.save(filename)
                print(f"💾 Saved as {filename}")
                
                # Convert to PIL Image for further processing
                pil_image = image.to_pil_image()
                print(f"🖼️  PIL Image size: {pil_image.size}")
                
            else:
                print("❌ Failed to capture image")
            
            # Wait between captures
            if i < 4:  # Don't wait after the last capture
                await asyncio.sleep(1)
        
        # Show performance stats
        stats = camera.get_performance_stats()
        print(f"\n📊 Performance Stats:")
        print(f"   Frames received: {stats['frames_received']}")
        print(f"   Bytes received: {stats['bytes_received']}")
        print(f"   Elapsed time: {stats.get('elapsed_time', 0):.1f}s")
        
    except Exception as e:
        print(f"❌ Error: {e}")
        import traceback
        traceback.print_exc()
        
    finally:
        # Cleanup
        await camera.disconnect()
        print("🔌 Disconnected from camera")

async def streaming_example():
    """Example of frame streaming"""
    print("\n📹 SidekickOS Camera Streaming Example")
    print("=" * 40)
    
    camera = ESP32Camera()
    frame_count = 0
    
    def frame_callback(frame):
        nonlocal frame_count
        frame_count += 1
        print(f"📺 Frame {frame_count}: {frame.size} bytes ({frame.completion_rate:.1f}%)")
        
        # Save every 5th frame
        if frame_count % 5 == 0:
            filename = f"stream_frame_{frame_count}.jpg"
            frame.save(filename)
            print(f"💾 Saved {filename}")
    
    try:
        # Connect and configure
        if not await camera.connect():
            print("❌ Failed to connect to camera")
            return
        
        print("✅ Connected for streaming!")
        
        # Start streaming
        print("🎬 Starting frame streaming for 15 seconds...")
        await camera.start_streaming(
            callback=frame_callback,
            interval=1.0,  # 1 frame per second
            quality=30
        )
        
        # Let it stream for 15 seconds
        await asyncio.sleep(15)
        
        # Stop streaming
        await camera.stop_streaming()
        print(f"⏹️  Streaming stopped. Captured {frame_count} frames.")
        
    except Exception as e:
        print(f"❌ Streaming error: {e}")
    finally:
        await camera.disconnect()

async def main():
    """Run both examples"""
    # Run basic capture example
    await basic_camera_example()
    
    # Wait a bit between examples
    await asyncio.sleep(2)
    
    # Run streaming example
    await streaming_example()
    
    print("\n🎉 Examples complete!")

if __name__ == "__main__":
    asyncio.run(main()) 