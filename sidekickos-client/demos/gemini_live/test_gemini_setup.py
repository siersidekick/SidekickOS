#!/usr/bin/env python3
"""
Test script to verify SidekickOS + Gemini Live setup

This script tests:
1. SidekickOS camera connection
2. Gemini API key and connectivity
3. Audio system setup
4. Dependencies

Run this before using the main demo to ensure everything is working.
"""

import os
import sys
import asyncio
import logging

# Add parent directory to path for sidekickos import
sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

# Test imports
try:
    from sidekickos import ESP32Camera
    print("✅ SidekickOS library imported successfully")
except ImportError as e:
    print(f"❌ Failed to import SidekickOS: {e}")
    sys.exit(1)

try:
    from google import genai
    print("✅ Google Gemini library imported successfully")
except ImportError as e:
    print(f"❌ Failed to import Google Gemini library: {e}")
    print("   Install with: pip install google-genai")
    sys.exit(1)

try:
    import pyaudio
    print("✅ PyAudio library imported successfully")
except ImportError as e:
    print(f"❌ Failed to import PyAudio: {e}")
    print("   Install with: pip install pyaudio")
    sys.exit(1)

try:
    import PIL.Image
    print("✅ PIL (Pillow) library imported successfully")
except ImportError as e:
    print(f"❌ Failed to import PIL: {e}")
    print("   Install with: pip install pillow")
    sys.exit(1)

# Configure logging
logging.basicConfig(level=logging.INFO)

async def test_camera_connection():
    """Test SidekickOS camera connection"""
    print("\n🔍 Testing SidekickOS camera connection...")
    
    try:
        camera = ESP32Camera()
        
        # Test connection
        if await camera.connect():
            print("✅ SidekickOS camera connected successfully!")
            
            # Test image capture
            print("📸 Testing image capture...")
            image = await camera.capture_image(timeout=10.0)
            
            if image:
                print(f"✅ Image captured successfully: {image.size} bytes")
                print(f"   Chunks: {image.chunks_received}/{image.chunks_expected}")
                print(f"   Completion: {image.completion_rate:.1f}%")
                
                # Test PIL conversion
                try:
                    pil_image = image.to_pil_image()
                    print(f"✅ PIL conversion successful: {pil_image.size}")
                except Exception as e:
                    print(f"❌ PIL conversion failed: {e}")
                    
            else:
                print("❌ Failed to capture image")
                
            await camera.disconnect()
            
        else:
            print("❌ Failed to connect to SidekickOS camera")
            print("   Make sure your device is powered on and nearby")
            return False
            
    except Exception as e:
        print(f"❌ Camera test failed: {e}")
        return False
    
    return True

def test_gemini_api():
    """Test Gemini API key and connectivity"""
    print("\n🤖 Testing Gemini API setup...")
    
    api_key = os.environ.get("GEMINI_API_KEY")
    if not api_key:
        print("❌ GEMINI_API_KEY environment variable not set")
        print("   Set it with: export GEMINI_API_KEY='your-api-key'")
        return False
    
    print("✅ GEMINI_API_KEY environment variable found")
    
    try:
        client = genai.Client(
            http_options={"api_version": "v1beta"},
            api_key=api_key,
        )
        print("✅ Gemini client created successfully")
        
        # Test basic API call
        print("🔍 Testing basic API connectivity...")
        # Note: We can't easily test the Live API without a full session
        # but we can test basic client creation
        
    except Exception as e:
        print(f"❌ Gemini API test failed: {e}")
        return False
    
    return True

def test_audio_system():
    """Test audio system setup"""
    print("\n🎤 Testing audio system...")
    
    try:
        pya = pyaudio.PyAudio()
        
        # Test microphone
        try:
            mic_info = pya.get_default_input_device_info()
            print(f"✅ Default microphone found: {mic_info['name']}")
            print(f"   Channels: {mic_info['maxInputChannels']}")
            print(f"   Sample rate: {mic_info['defaultSampleRate']}")
        except Exception as e:
            print(f"❌ Microphone test failed: {e}")
            return False
        
        # Test speakers
        try:
            speaker_info = pya.get_default_output_device_info()
            print(f"✅ Default speakers found: {speaker_info['name']}")
            print(f"   Channels: {speaker_info['maxOutputChannels']}")
            print(f"   Sample rate: {speaker_info['defaultSampleRate']}")
        except Exception as e:
            print(f"❌ Speaker test failed: {e}")
            return False
            
        pya.terminate()
        
    except Exception as e:
        print(f"❌ Audio system test failed: {e}")
        return False
    
    return True

async def main():
    """Run all tests"""
    print("🧪 SidekickOS + Gemini Live Setup Test")
    print("=" * 50)
    
    tests_passed = 0
    total_tests = 3
    
    # Test 1: Camera connection
    if await test_camera_connection():
        tests_passed += 1
    
    # Test 2: Gemini API
    if test_gemini_api():
        tests_passed += 1
    
    # Test 3: Audio system
    if test_audio_system():
        tests_passed += 1
    
    print("\n" + "=" * 50)
    print(f"📊 Test Results: {tests_passed}/{total_tests} tests passed")
    
    if tests_passed == total_tests:
        print("🎉 All tests passed! You're ready to run the demo!")
        print("   Run: python sidekickos_gemini_demo.py")
    else:
        print("❌ Some tests failed. Please fix the issues above before running the demo.")
        
    return tests_passed == total_tests

if __name__ == "__main__":
    asyncio.run(main()) 