#!/usr/bin/env python3
"""
SidekickOS + Gemini Live Demo

A smart glasses demo that uses the SidekickOS BLE camera with Google's Gemini Live API
for real-time computer vision and audio interaction.

Setup:
1. Set your GEMINI_API_KEY environment variable
2. Install dependencies: pip install google-genai pyaudio pillow
3. Make sure your SidekickOS device is powered on and nearby

Usage:
python sidekickos_gemini_demo.py
"""

import os
import asyncio
import base64
import io
import traceback
import logging

import pyaudio
import PIL.Image

from google import genai
from google.genai import types

import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))
from sidekickos import ESP32Camera

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

FORMAT = pyaudio.paInt16
CHANNELS = 1
SEND_SAMPLE_RATE = 16000
RECEIVE_SAMPLE_RATE = 24000
CHUNK_SIZE = 1024

MODEL = "models/gemini-2.5-flash-preview-native-audio-dialog"

client = genai.Client(
    http_options={"api_version": "v1beta"},
    api_key=os.environ.get("GEMINI_API_KEY"),
)

tools = [
    types.Tool(google_search=types.GoogleSearch()),
]

CONFIG = types.LiveConnectConfig(
    response_modalities=[
        "AUDIO",
    ],
    media_resolution="MEDIA_RESOLUTION_MEDIUM",
    speech_config=types.SpeechConfig(
        voice_config=types.VoiceConfig(
            prebuilt_voice_config=types.PrebuiltVoiceConfig(voice_name="Zephyr")
        )
    ),
    context_window_compression=types.ContextWindowCompressionConfig(
        trigger_tokens=25600,
        sliding_window=types.SlidingWindow(target_tokens=12800),
    ),
    tools=tools,
)

pya = pyaudio.PyAudio()


class SidekickOSGeminiDemo:
    def __init__(self):
        self.camera = None
        self.audio_in_queue = None
        self.out_queue = None
        self.session = None
        self.audio_stream = None
        self.frame_count = 0
        
    async def connect_camera(self):
        """Connect to SidekickOS BLE camera"""
        print("🔍 Connecting to SidekickOS camera...")
        self.camera = ESP32Camera()
        
        if not await self.camera.connect():
            raise RuntimeError("Failed to connect to SidekickOS camera")
        
        print("📷 SidekickOS camera connected successfully!")
        
        # Set optimal settings for real-time streaming
        await self.camera.set_quality(30)  # Good quality for Gemini
        await self.camera.set_resolution(5)  # QVGA (320x240) for good performance
        
        print("⚙️  Camera settings configured")

    async def send_text(self):
        """Handle text input from user"""
        while True:
            text = await asyncio.to_thread(
                input,
                "💬 message > ",
            )
            if text.lower() == "q":
                break
            await self.session.send(input=text or ".", end_of_turn=True)

    async def capture_frame(self):
        """Capture a single frame from SidekickOS camera"""
        try:
            # Capture image from BLE camera
            image_frame = await self.camera.capture_image(timeout=5.0)
            
            if not image_frame:
                logger.warning("Failed to capture image from SidekickOS camera")
                return None
                
            # Convert to PIL Image
            pil_image = image_frame.to_pil_image()
            
            # Resize for optimal processing
            pil_image.thumbnail([1024, 1024])
            
            # Convert to base64 JPEG
            image_io = io.BytesIO()
            pil_image.save(image_io, format="jpeg")
            image_io.seek(0)
            
            image_bytes = image_io.read()
            
            self.frame_count += 1
            logger.info(f"📸 Captured frame {self.frame_count}: {len(image_bytes)} bytes")
            
            return {
                "mime_type": "image/jpeg", 
                "data": base64.b64encode(image_bytes).decode()
            }
            
        except Exception as e:
            logger.error(f"Error capturing frame: {e}")
            return None

    async def get_frames(self):
        """Continuously capture frames from SidekickOS camera"""
        print("📹 Starting frame capture from SidekickOS camera...")
        
        while True:
            frame = await self.capture_frame()
            if frame is None:
                await asyncio.sleep(0.5)  # Wait before retrying
                continue
                
            await self.out_queue.put(frame)
            
            # Capture every 2 seconds to avoid overwhelming the API
            await asyncio.sleep(2.0)

    async def send_realtime(self):
        """Send captured frames to Gemini Live API"""
        while True:
            msg = await self.out_queue.get()
            await self.session.send(input=msg)

    async def listen_audio(self):
        """Capture audio from microphone"""
        print("🎤 Starting audio capture...")
        
        mic_info = pya.get_default_input_device_info()
        self.audio_stream = await asyncio.to_thread(
            pya.open,
            format=FORMAT,
            channels=CHANNELS,
            rate=SEND_SAMPLE_RATE,
            input=True,
            input_device_index=mic_info["index"],
            frames_per_buffer=CHUNK_SIZE,
        )
        
        if __debug__:
            kwargs = {"exception_on_overflow": False}
        else:
            kwargs = {}
            
        while True:
            data = await asyncio.to_thread(self.audio_stream.read, CHUNK_SIZE, **kwargs)
            await self.out_queue.put({"data": data, "mime_type": "audio/pcm"})

    async def receive_audio(self):
        """Receive audio responses from Gemini Live API"""
        print("🔊 Starting audio reception...")
        
        while True:
            turn = self.session.receive()
            async for response in turn:
                if data := response.data:
                    self.audio_in_queue.put_nowait(data)
                    continue
                if text := response.text:
                    print(f"🤖 Gemini: {text}")

            # Handle interruptions by clearing audio queue
            while not self.audio_in_queue.empty():
                self.audio_in_queue.get_nowait()

    async def play_audio(self):
        """Play audio responses from Gemini"""
        print("🔈 Starting audio playback...")
        
        stream = await asyncio.to_thread(
            pya.open,
            format=FORMAT,
            channels=CHANNELS,
            rate=RECEIVE_SAMPLE_RATE,
            output=True,
        )
        
        while True:
            bytestream = await self.audio_in_queue.get()
            await asyncio.to_thread(stream.write, bytestream)

    async def run(self):
        """Main demo loop"""
        try:
            # Connect to SidekickOS camera first
            await self.connect_camera()
            
            # Send initial context to Gemini
            initial_context = """
            You are an AI assistant integrated with SidekickOS smart glasses. 
            You can see what the user sees through their camera and respond with audio.
            Be helpful, concise, and engaging. You can:
            - Describe what you see in the camera feed
            - Answer questions about the environment
            - Provide contextual information
            - Help with tasks based on visual input
            
            The user is wearing smart glasses with a camera, so respond as if you're their AI companion.
            """
            
            async with (
                client.aio.live.connect(model=MODEL, config=CONFIG) as session,
                asyncio.TaskGroup() as tg,
            ):
                self.session = session
                
                # Send initial context
                await self.session.send(input=initial_context, end_of_turn=True)
                
                self.audio_in_queue = asyncio.Queue()
                self.out_queue = asyncio.Queue(maxsize=5)

                # Start all tasks
                print("🚀 Starting SidekickOS + Gemini Live demo...")
                print("💡 Tips:")
                print("   - Speak naturally to interact with Gemini")
                print("   - Type messages in the console if needed")
                print("   - Type 'q' to quit")
                print("   - Gemini can see what your camera sees!")
                
                send_text_task = tg.create_task(self.send_text())
                tg.create_task(self.send_realtime())
                tg.create_task(self.listen_audio())
                tg.create_task(self.get_frames())
                tg.create_task(self.receive_audio())
                tg.create_task(self.play_audio())

                await send_text_task
                raise asyncio.CancelledError("User requested exit")

        except asyncio.CancelledError:
            print("👋 Shutting down...")
        except Exception as e:
            print(f"❌ Error: {e}")
            traceback.print_exc()
        finally:
            # Cleanup
            if self.audio_stream:
                self.audio_stream.close()
            if self.camera:
                await self.camera.disconnect()
            print("🔌 Disconnected from SidekickOS camera")


async def main():
    """Main entry point"""
    # Check for API key
    if not os.environ.get("GEMINI_API_KEY"):
        print("❌ Error: GEMINI_API_KEY environment variable not set")
        print("   Please set it with: export GEMINI_API_KEY='your-api-key'")
        return
    
    demo = SidekickOSGeminiDemo()
    await demo.run()


if __name__ == "__main__":
    asyncio.run(main()) 