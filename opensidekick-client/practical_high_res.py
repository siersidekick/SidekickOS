#!/usr/bin/env python3
"""
Practical High-Resolution Image Capture

This script tests different resolutions to find what actually works with your ESP32S3 camera
and captures the highest quality images possible, then upscales to 1920x1080.

Strategy:
1. Test resolutions from highest to lowest
2. Find the maximum working resolution
3. Capture at that resolution with optimal settings
4. Upscale to 1920x1080 if needed

Requirements:
    pip install bleak pillow

Usage:
    python practical_high_res.py
"""

import asyncio
import os
import time
from datetime import datetime
from PIL import Image, ImageEnhance, ImageFilter
from esp32_ble_camera import ESP32Camera, ImageFrame


class PracticalHighResCapture:
    """Practical high-resolution capture that works within ESP32 limitations"""
    
    def __init__(self):
        self.camera = ESP32Camera()
        self.output_dir = f"practical_hr_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
        os.makedirs(self.output_dir, exist_ok=True)
        
        # Resolution test list (from highest to lowest)
        self.resolutions = [
            (8, "VGA", "640x480", "Good balance of quality and reliability"),
            (9, "SVGA", "800x600", "Higher quality if supported"),
            (10, "XGA", "1024x768", "Very high quality"),
            (11, "HD", "1280x720", "HD video format"),
            (12, "SXGA", "1280x1024", "Very high resolution"),
            (13, "UXGA", "1600x1200", "Maximum theoretical resolution"),
        ]
        
        self.working_resolution = None
        
    async def connect(self):
        """Connect to camera"""
        print("🔍 Connecting to OpenSidekick camera...")
        if not await self.camera.connect():
            print("❌ Failed to connect to camera")
            return False
        print("✅ Connected successfully!")
        return True
    
    async def disconnect(self):
        """Disconnect from camera"""
        await self.camera.disconnect()
    
    async def test_resolution(self, size_code: int, name: str, dimensions: str, timeout: float = 15.0) -> bool:
        """Test if a resolution works"""
        print(f"🧪 Testing {name} ({dimensions})...")
        
        try:
            # Set resolution and moderate quality for testing
            await self.camera.set_resolution(size_code)
            await self.camera.set_quality(20)  # Moderate quality for faster testing
            
            # Give camera time to adjust
            await asyncio.sleep(2)
            
            # Try to capture
            image = await self.camera.capture_image(timeout=timeout)
            
            if image and image.size > 1000:  # Minimum reasonable size
                print(f"   ✅ {name} works! ({image.size:,} bytes, {image.completion_rate:.1f}%)")
                return True
            else:
                print(f"   ❌ {name} failed or too small")
                return False
                
        except Exception as e:
            print(f"   ❌ {name} failed: {e}")
            return False
    
    async def find_best_resolution(self):
        """Find the highest working resolution"""
        print("🔍 TESTING RESOLUTIONS (highest to lowest)")
        print("=" * 50)
        
        # Start with a known working resolution (VGA)
        working_resolutions = []
        
        # Test VGA first (most likely to work)
        if await self.test_resolution(8, "VGA", "640x480", 10.0):
            working_resolutions.append((8, "VGA", "640x480"))
        
        # Test higher resolutions
        for size_code, name, dimensions, description in self.resolutions[1:]:  # Skip VGA since we tested it
            if await self.test_resolution(size_code, name, dimensions, 20.0):
                working_resolutions.append((size_code, name, dimensions))
            else:
                # If a resolution fails, don't test higher ones
                print(f"   ⚠️  Stopping at {name} - higher resolutions likely won't work")
                break
        
        if working_resolutions:
            # Use the highest working resolution
            self.working_resolution = working_resolutions[-1]
            size_code, name, dimensions = self.working_resolution
            print(f"\n🎯 Best resolution found: {name} ({dimensions})")
            return True
        else:
            print("\n❌ No working resolutions found!")
            return False
    
    async def capture_best_quality(self) -> ImageFrame:
        """Capture image at best quality using the working resolution"""
        if not self.working_resolution:
            return None
            
        size_code, name, dimensions = self.working_resolution
        
        print(f"\n📷 Capturing at BEST QUALITY: {name} ({dimensions})")
        
        # Set best quality settings
        await self.camera.set_resolution(size_code)
        await self.camera.set_quality(4)  # Highest quality
        
        # Give camera time to adjust
        await asyncio.sleep(2)
        
        print("   📸 Taking high-quality photo...")
        # Longer timeout for high quality
        image = await self.camera.capture_image(timeout=30.0)
        
        if image:
            print(f"   ✅ Captured: {image.size:,} bytes ({image.completion_rate:.1f}%)")
            return image
        else:
            print("   ❌ High-quality capture failed")
            return None
    
    def smart_upscale_to_1920x1080(self, image_frame: ImageFrame) -> Image.Image:
        """Smart upscaling that chooses the best method based on source resolution"""
        pil_image = image_frame.to_pil_image()
        original_size = pil_image.size
        target_size = (1920, 1080)
        
        scale_factor = target_size[0] / original_size[0]
        
        print(f"\n🔍 Smart upscaling: {original_size[0]}x{original_size[1]} → {target_size[0]}x{target_size[1]}")
        print(f"   Scale factor: {scale_factor:.2f}x")
        
        # Choose algorithm based on scale factor
        if scale_factor <= 1.5:
            # Small upscale - use Lanczos for best quality
            method = Image.LANCZOS
            method_name = "LANCZOS (best quality)"
        elif scale_factor <= 2.0:
            # Medium upscale - use Bicubic
            method = Image.BICUBIC
            method_name = "BICUBIC (good balance)"
        else:
            # Large upscale - use Bicubic with pre-sharpening
            method = Image.BICUBIC
            method_name = "BICUBIC with pre-sharpening"
            # Pre-sharpen for large upscales
            pil_image = pil_image.filter(ImageFilter.UnsharpMask(radius=0.5, percent=100, threshold=2))
        
        print(f"   Using: {method_name}")
        
        # Perform upscale
        upscaled = pil_image.resize(target_size, method)
        
        return upscaled
    
    def enhance_for_1920x1080(self, image: Image.Image) -> Image.Image:
        """Apply enhancements optimized for 1920x1080 output"""
        print("   🎨 Applying 1920x1080 optimizations...")
        
        # Moderate sharpening (good for upscaled images)
        enhanced = image.filter(ImageFilter.UnsharpMask(radius=1.0, percent=110, threshold=3))
        
        # Slight contrast boost
        contrast_enhancer = ImageEnhance.Contrast(enhanced)
        enhanced = contrast_enhancer.enhance(1.08)
        
        # Very slight color boost
        color_enhancer = ImageEnhance.Color(enhanced)
        enhanced = color_enhancer.enhance(1.03)
        
        # Optional: Reduce noise for very upscaled images
        # enhanced = enhanced.filter(ImageFilter.MedianFilter(size=3))
        
        print("   ✅ Optimizations applied")
        return enhanced
    
    def save_high_quality(self, image: Image.Image, prefix: str) -> str:
        """Save image with maximum quality settings"""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"{prefix}_{timestamp}.jpg"
        filepath = os.path.join(self.output_dir, filename)
        
        # Save with maximum quality
        image.save(filepath, "JPEG", quality=98, optimize=True, progressive=True)
        
        file_size = os.path.getsize(filepath)
        print(f"   💾 Saved: {filename} ({file_size:,} bytes)")
        return filepath
    
    async def create_1920x1080_image(self):
        """Complete workflow to create a 1920x1080 image"""
        print("🎯 CREATING 1920x1080 IMAGE")
        print("=" * 40)
        
        # Step 1: Find best resolution
        if not await self.find_best_resolution():
            return None
        
        # Step 2: Capture at best quality
        image = await self.capture_best_quality()
        if not image:
            return None
        
        # Step 3: Save original
        pil_original = image.to_pil_image()
        original_path = self.save_high_quality(pil_original, "01_original")
        
        # Step 4: Smart upscale to 1920x1080
        upscaled = self.smart_upscale_to_1920x1080(image)
        
        # Step 5: Apply enhancements
        enhanced = self.enhance_for_1920x1080(upscaled)
        
        # Step 6: Save final 1920x1080 image
        final_path = self.save_high_quality(enhanced, "02_final_1920x1080")
        
        # Summary
        print(f"\n🎉 SUCCESS!")
        print(f"📊 Source: {self.working_resolution[1]} ({self.working_resolution[2]})")
        print(f"📊 Final: 1920x1080")
        print(f"📁 Original: {os.path.basename(original_path)}")
        print(f"📁 Final: {os.path.basename(final_path)}")
        print(f"📂 Directory: {self.output_dir}")
        
        return final_path


async def quick_test():
    """Quick test to see what resolutions work"""
    print("🧪 QUICK RESOLUTION TEST")
    print("=" * 30)
    
    capture = PracticalHighResCapture()
    
    try:
        if not await capture.connect():
            return
        
        # Test a few key resolutions quickly
        test_resolutions = [
            (5, "QVGA", "320x240"),
            (8, "VGA", "640x480"),
            (9, "SVGA", "800x600"),
            (10, "XGA", "1024x768"),
        ]
        
        working = []
        for size_code, name, dimensions in test_resolutions:
            if await capture.test_resolution(size_code, name, dimensions, 10.0):
                working.append((name, dimensions))
        
        print(f"\n📊 Working resolutions:")
        for name, dimensions in working:
            print(f"   ✅ {name} ({dimensions})")
        
        if working:
            print(f"\n💡 Recommended: Use option 2 to create 1920x1080 from {working[-1][0]}")
        
    finally:
        await capture.disconnect()


async def main():
    """Main function"""
    print("🎯 PRACTICAL HIGH-RESOLUTION CAPTURE")
    print("=" * 45)
    print("This script finds the best resolution your camera supports")
    print("and creates optimized 1920x1080 images.")
    print()
    
    while True:
        print("Options:")
        print("1. Quick resolution test")
        print("2. Create 1920x1080 image (full process)")
        print("3. Exit")
        
        try:
            choice = input("\nEnter choice (1-3): ").strip()
            
            if choice == '1':
                await quick_test()
                
            elif choice == '2':
                capture = PracticalHighResCapture()
                try:
                    if await capture.connect():
                        await capture.create_1920x1080_image()
                finally:
                    await capture.disconnect()
                    
            elif choice == '3':
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