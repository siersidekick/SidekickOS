#!/usr/bin/env python3
"""
High-Resolution Image Capture Example

This script captures images at the maximum resolution supported by the ESP32S3 camera
and optionally upscales them to 1920x1080 using advanced image processing.

Supported ESP32 resolutions:
- Size 13: UXGA (1600x1200) - Maximum native resolution
- Size 12: SXGA (1280x1024) 
- Size 11: HD (1280x720) - HD video format
- Size 10: XGA (1024x768)
- Size 9: SVGA (800x600)

Requirements:
    pip install bleak pillow

Usage:
    python high_res_capture.py
"""

import asyncio
import os
import time
from datetime import datetime
from PIL import Image, ImageEnhance, ImageFilter
from opensidekick import ESP32Camera, ImageFrame


class HighResolutionCapture:
    """High-resolution image capture with advanced processing"""
    
    def __init__(self):
        self.camera = ESP32Camera()
        self.output_dir = f"high_res_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
        os.makedirs(self.output_dir, exist_ok=True)
        
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
    
    async def capture_maximum_resolution(self, quality: int = 4) -> ImageFrame:
        """Capture image at maximum supported resolution"""
        print(f"\n📷 Capturing at MAXIMUM resolution (UXGA 1600x1200)")
        print(f"   Quality: {quality} (lower = better)")
        
        # Set maximum resolution and highest quality
        await self.camera.set_resolution(13)  # UXGA (1600x1200)
        await self.camera.set_quality(quality)  # Highest quality
        
        # Give camera time to adjust
        await asyncio.sleep(1)
        
        print("   📸 Taking photo...")
        image = await self.camera.capture_image(timeout=30.0)  # Longer timeout for large images
        
        if image:
            print(f"   ✅ Captured: {image.size:,} bytes ({image.completion_rate:.1f}%)")
            return image
        else:
            print("   ❌ Capture failed")
            return None
    
    async def capture_hd_resolution(self, quality: int = 6) -> ImageFrame:
        """Capture image at HD resolution (1280x720)"""
        print(f"\n📷 Capturing at HD resolution (1280x720)")
        print(f"   Quality: {quality}")
        
        # Set HD resolution
        await self.camera.set_resolution(11)  # HD (1280x720)
        await self.camera.set_quality(quality)
        
        await asyncio.sleep(1)
        
        print("   📸 Taking photo...")
        image = await self.camera.capture_image(timeout=20.0)
        
        if image:
            print(f"   ✅ Captured: {image.size:,} bytes ({image.completion_rate:.1f}%)")
            return image
        else:
            print("   ❌ Capture failed")
            return None
    
    def upscale_to_1920x1080(self, image_frame: ImageFrame, method: str = "lanczos") -> Image.Image:
        """Upscale image to 1920x1080 using advanced algorithms"""
        print(f"\n🔍 Upscaling to 1920x1080 using {method.upper()} algorithm...")
        
        # Convert to PIL Image
        pil_image = image_frame.to_pil_image()
        original_size = pil_image.size
        print(f"   Original: {original_size[0]}x{original_size[1]}")
        
        # Choose resampling method
        resample_methods = {
            "lanczos": Image.LANCZOS,      # Best quality, slower
            "bicubic": Image.BICUBIC,      # Good quality, medium speed
            "bilinear": Image.BILINEAR,    # Fast, decent quality
            "nearest": Image.NEAREST       # Fastest, pixelated
        }
        
        resample = resample_methods.get(method.lower(), Image.LANCZOS)
        
        # Upscale to 1920x1080
        target_size = (1920, 1080)
        upscaled = pil_image.resize(target_size, resample)
        
        print(f"   Upscaled: {target_size[0]}x{target_size[1]}")
        print(f"   Scale factor: {target_size[0]/original_size[0]:.2f}x")
        
        return upscaled
    
    def enhance_image(self, image: Image.Image) -> Image.Image:
        """Apply image enhancements for better quality"""
        print("   🎨 Applying image enhancements...")
        
        # Sharpen the image
        enhanced = image.filter(ImageFilter.UnsharpMask(radius=1, percent=120, threshold=3))
        
        # Enhance contrast slightly
        contrast_enhancer = ImageEnhance.Contrast(enhanced)
        enhanced = contrast_enhancer.enhance(1.1)
        
        # Enhance color saturation slightly
        color_enhancer = ImageEnhance.Color(enhanced)
        enhanced = color_enhancer.enhance(1.05)
        
        print("   ✅ Enhancements applied")
        return enhanced
    
    def save_image(self, image: Image.Image, prefix: str, suffix: str = "") -> str:
        """Save image with timestamp"""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"{prefix}_{timestamp}{suffix}.jpg"
        filepath = os.path.join(self.output_dir, filename)
        
        # Save with high quality
        image.save(filepath, "JPEG", quality=95, optimize=True)
        
        file_size = os.path.getsize(filepath)
        print(f"   💾 Saved: {filename} ({file_size:,} bytes)")
        return filepath
    
    async def capture_and_process_series(self):
        """Capture a series of high-resolution images with different processing"""
        print("📸 HIGH-RESOLUTION IMAGE CAPTURE SERIES")
        print("=" * 50)
        
        results = []
        
        # 1. Maximum resolution capture
        try:
            max_res_image = await self.capture_maximum_resolution(quality=4)
            if max_res_image:
                # Save original
                max_res_image.save(os.path.join(self.output_dir, "01_max_resolution_original.jpg"))
                
                # Convert to PIL and save enhanced version
                pil_original = max_res_image.to_pil_image()
                enhanced_original = self.enhance_image(pil_original)
                self.save_image(enhanced_original, "02_max_resolution", "_enhanced")
                
                # Upscale to 1920x1080
                upscaled_lanczos = self.upscale_to_1920x1080(max_res_image, "lanczos")
                upscaled_enhanced = self.enhance_image(upscaled_lanczos)
                self.save_image(upscaled_enhanced, "03_upscaled_1920x1080", "_lanczos_enhanced")
                
                results.append(("Maximum Resolution (1600x1200)", max_res_image.size, True))
            else:
                results.append(("Maximum Resolution (1600x1200)", 0, False))
        except Exception as e:
            print(f"❌ Maximum resolution capture failed: {e}")
            results.append(("Maximum Resolution (1600x1200)", 0, False))
        
        # 2. HD resolution capture
        try:
            hd_image = await self.capture_hd_resolution(quality=6)
            if hd_image:
                # Save original
                hd_image.save(os.path.join(self.output_dir, "04_hd_original.jpg"))
                
                # Upscale HD to 1920x1080 (smaller upscale factor)
                upscaled_hd = self.upscale_to_1920x1080(hd_image, "lanczos")
                upscaled_hd_enhanced = self.enhance_image(upscaled_hd)
                self.save_image(upscaled_hd_enhanced, "05_hd_upscaled_1920x1080", "_enhanced")
                
                results.append(("HD Resolution (1280x720)", hd_image.size, True))
            else:
                results.append(("HD Resolution (1280x720)", 0, False))
        except Exception as e:
            print(f"❌ HD resolution capture failed: {e}")
            results.append(("HD Resolution (1280x720)", 0, False))
        
        # 3. Multiple upscaling methods comparison
        if max_res_image:
            print(f"\n🔬 Comparing upscaling methods...")
            methods = ["lanczos", "bicubic", "bilinear"]
            
            for method in methods:
                upscaled = self.upscale_to_1920x1080(max_res_image, method)
                self.save_image(upscaled, f"06_comparison_{method}", "_1920x1080")
        
        return results
    
    def print_summary(self, results):
        """Print capture summary"""
        print(f"\n📊 CAPTURE SUMMARY")
        print("=" * 50)
        print(f"Output directory: {self.output_dir}")
        print(f"Images saved: {len([f for f in os.listdir(self.output_dir) if f.endswith('.jpg')])}")
        print()
        
        for description, size, success in results:
            status = "✅ SUCCESS" if success else "❌ FAILED"
            size_str = f"{size:,} bytes" if success else "N/A"
            print(f"{description}: {status} ({size_str})")
        
        print(f"\n📁 All images saved to: {self.output_dir}/")
        print("\nFiles created:")
        for filename in sorted(os.listdir(self.output_dir)):
            if filename.endswith('.jpg'):
                filepath = os.path.join(self.output_dir, filename)
                file_size = os.path.getsize(filepath)
                print(f"  📷 {filename} ({file_size:,} bytes)")


async def quick_1920x1080_capture():
    """Quick function to capture and upscale to 1920x1080"""
    print("🚀 QUICK 1920x1080 CAPTURE")
    print("=" * 30)
    
    capture = HighResolutionCapture()
    
    try:
        if not await capture.connect():
            return
        
        # Capture at maximum resolution
        image = await capture.capture_maximum_resolution(quality=6)
        
        if image:
            # Upscale to 1920x1080
            upscaled = capture.upscale_to_1920x1080(image, "lanczos")
            enhanced = capture.enhance_image(upscaled)
            
            # Save
            filename = capture.save_image(enhanced, "quick_1920x1080", "_enhanced")
            
            print(f"\n✅ 1920x1080 image ready!")
            print(f"📁 Saved as: {filename}")
            
            # Show image info
            print(f"📊 Final image: {enhanced.size[0]}x{enhanced.size[1]}")
            print(f"💾 File size: {os.path.getsize(filename):,} bytes")
        
    finally:
        await capture.disconnect()


async def main():
    """Main function with options"""
    print("🎯 HIGH-RESOLUTION CAMERA CAPTURE")
    print("=" * 40)
    print("This script captures high-resolution images from your OpenSidekick camera")
    print("and can upscale them to 1920x1080 using advanced algorithms.")
    print()
    
    while True:
        print("Options:")
        print("1. Quick 1920x1080 capture")
        print("2. Full high-resolution series")
        print("3. Exit")
        
        try:
            choice = input("\nEnter choice (1-3): ").strip()
            
            if choice == '1':
                await quick_1920x1080_capture()
                
            elif choice == '2':
                capture = HighResolutionCapture()
                try:
                    if await capture.connect():
                        results = await capture.capture_and_process_series()
                        capture.print_summary(results)
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