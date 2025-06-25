"""
ESP32S3 BLE Camera Interface Module

A Python module for interfacing with ESP32S3 BLE camera for image capture and streaming.
Compatible with Mac and PC using the bleak library.

Usage:
    from esp32_ble_camera import ESP32Camera
    
    camera = ESP32Camera()
    await camera.connect()
    image_data = await camera.capture_image()
    await camera.start_streaming(callback=my_image_callback)
"""

import asyncio
import logging
import time
from typing import Optional, Callable, Dict, Any
from dataclasses import dataclass
from io import BytesIO
from PIL import Image
import bleak
from bleak import BleakClient, BleakScanner
from bleak.backends.characteristic import BleakGATTCharacteristic

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# BLE UUIDs - Handle both 16-bit and 128-bit UUIDs
SERVICE_UUID_128 = "12345678-1234-1234-1234-123456789abc"  # 128-bit UUID
SERVICE_UUID_16 = "1234"  # 16-bit UUID (what ESP32 actually uses)
CONTROL_CHAR_UUID = "87654321-4321-4321-4321-cba987654321"
STATUS_CHAR_UUID = "11111111-2222-3333-4444-555555555555"
IMAGE_CHAR_UUID = "22222222-3333-4444-5555-666666666666"
FRAME_CHAR_UUID = "44444444-5555-6666-7777-888888888888"
AUDIO_CHAR_UUID = "33333333-4444-5555-6666-777777777777"

# Constants
MAX_CHUNK_SIZE = 510  # Updated for ultra-speed optimization
DEVICE_NAME = "OpenSidekick"  # Updated device name


@dataclass
class ImageFrame:
    """Represents a received image frame"""
    data: bytes
    size: int
    chunks_received: int
    chunks_expected: int
    timestamp: float
    frame_number: int
    
    def to_pil_image(self) -> Image.Image:
        """Convert image data to PIL Image"""
        return Image.open(BytesIO(self.data))
    
    def save(self, filename: str):
        """Save image to file"""
        with open(filename, 'wb') as f:
            f.write(self.data)
    
    @property
    def completion_rate(self) -> float:
        """Get completion rate as percentage"""
        return (self.chunks_received / self.chunks_expected) * 100 if self.chunks_expected > 0 else 0


class ESP32Camera:
    """ESP32S3 BLE Camera Interface"""
    
    def __init__(self, device_name: str = DEVICE_NAME):
        self.device_name = device_name
        self.client: Optional[BleakClient] = None
        self.connected = False
        
        # Characteristics
        self.control_char: Optional[BleakGATTCharacteristic] = None
        self.status_char: Optional[BleakGATTCharacteristic] = None
        self.image_char: Optional[BleakGATTCharacteristic] = None
        self.frame_char: Optional[BleakGATTCharacteristic] = None
        
        # Image reception state
        self.image_buffer: Optional[bytearray] = None
        self.expected_chunks = 0
        self.expected_size = 0
        self.received_chunks = 0
        self.current_frame_number = 0
        self.is_streaming = False
        
        # Callbacks
        self.image_callback: Optional[Callable[[ImageFrame], None]] = None
        self.status_callback: Optional[Callable[[Dict[str, Any]], None]] = None
        
        # Performance tracking
        self.performance_stats = {
            'frames_received': 0,
            'bytes_received': 0,
            'start_time': None,
            'last_frame_time': None
        }
    
    async def scan_for_device(self, timeout: float = 10.0) -> Optional[str]:
        """Scan for OpenSidekick camera device"""
        logger.info(f"Scanning for camera device...")
        
        devices = await BleakScanner.discover(timeout=timeout)
        
        # Look for both possible device names
        possible_names = ["OpenSidekick", "ESP32S3-Camera"]
        
        for device in devices:
            if device.name:
                for name in possible_names:
                    if name in device.name:
                        logger.info(f"Found camera device: {device.name} ({device.address})")
                        return device.address
                logger.debug(f"Found other device: {device.name}")
        
        logger.warning(f"Camera device not found. Searched for: {possible_names}")
        logger.info("Available devices:")
        for device in devices:
            if device.name:
                logger.info(f"  - {device.name}")
        return None
    
    async def connect(self, device_address: Optional[str] = None, timeout: float = 10.0) -> bool:
        """Connect to OpenSidekick camera"""
        try:
            if not device_address:
                device_address = await self.scan_for_device(timeout)
                if not device_address:
                    return False
            
            logger.info(f"Connecting to {device_address}...")
            self.client = BleakClient(device_address)
            
            await self.client.connect()
            logger.info("Connected to GATT server")
            
            # Wait for connection to stabilize
            await asyncio.sleep(1.0)
            
            # Log MTU information
            if hasattr(self.client, 'mtu_size'):
                mtu = self.client.mtu_size
                logger.info(f"MTU: {mtu} bytes (DLE {'ENABLED' if mtu > 23 else 'DISABLED'})")
                if mtu >= 517:
                    logger.info("🚀 ULTRA-SPEED MODE: Maximum 517-byte MTU detected!")
            
            # Get service and characteristics
            logger.info("Getting BLE service...")
            services = self.client.services  # Use services property instead of deprecated get_services()
            service = None
            
            # Try to find service by both 16-bit and 128-bit UUIDs
            for svc in services:
                svc_uuid = str(svc.uuid).lower()
                if (svc_uuid == SERVICE_UUID_128.lower() or 
                    svc_uuid == SERVICE_UUID_16.lower() or
                    SERVICE_UUID_16.lower() in svc_uuid):
                    service = svc
                    logger.info(f"Found service: {svc.uuid}")
                    break
            
            if not service:
                logger.error(f"Service not found. Available services:")
                for svc in services:
                    logger.error(f"  - {svc.uuid}")
                return False
            
            logger.info("Getting characteristics...")
            self.control_char = service.get_characteristic(CONTROL_CHAR_UUID)
            self.status_char = service.get_characteristic(STATUS_CHAR_UUID)
            self.image_char = service.get_characteristic(IMAGE_CHAR_UUID)
            self.frame_char = service.get_characteristic(FRAME_CHAR_UUID)
            
            if not all([self.control_char, self.status_char, self.image_char, self.frame_char]):
                logger.error("Required characteristics not found")
                return False
            
            # Start notifications
            logger.info("Starting notifications...")
            await self.client.start_notify(self.status_char, self._handle_status_data)
            await self.client.start_notify(self.image_char, self._handle_image_data)
            await self.client.start_notify(self.frame_char, self._handle_frame_data)
            
            self.connected = True
            self.performance_stats['start_time'] = time.time()
            
            logger.info("🎉 Successfully connected to OpenSidekick Ultra-Performance Camera!")
            logger.info(f"⚡ Ready for 517-byte MTU and {MAX_CHUNK_SIZE}-byte chunks")
            
            # Get initial status
            await self.send_command("STATUS")
            
            return True
            
        except Exception as e:
            logger.error(f"Connection failed: {e}")
            return False
    
    async def disconnect(self):
        """Disconnect from camera"""
        if self.client and self.connected:
            await self.client.disconnect()
            self.connected = False
            logger.info("Disconnected from OpenSidekick camera")
    
    async def send_command(self, command: str) -> bool:
        """Send command to ESP32S3"""
        if not self.connected or not self.control_char:
            logger.error("Not connected to device")
            return False
        
        try:
            await self.client.write_gatt_char(self.control_char, command.encode())
            logger.debug(f"Sent command: {command}")
            return True
        except Exception as e:
            logger.error(f"Failed to send command '{command}': {e}")
            return False
    
    async def capture_image(self, timeout: float = 10.0) -> Optional[ImageFrame]:
        """Capture a single image"""
        logger.info("📷 Capturing image...")
        
        # Set up single image capture
        self.image_buffer = None
        self.is_streaming = False
        
        # Send capture command
        if not await self.send_command("CAPTURE"):
            return None
        
        # Wait for image with timeout
        start_time = time.time()
        while time.time() - start_time < timeout:
            if self.image_buffer and self.received_chunks >= self.expected_chunks * 0.95:
                # Image received successfully
                image_data = bytes(self.image_buffer[:self.expected_size])
                frame = ImageFrame(
                    data=image_data,
                    size=len(image_data),
                    chunks_received=self.received_chunks,
                    chunks_expected=self.expected_chunks,
                    timestamp=time.time(),
                    frame_number=self.current_frame_number
                )
                
                logger.info(f"✅ Image captured: {len(image_data)} bytes ({frame.completion_rate:.1f}%)")
                self._reset_image_state()
                return frame
            
            await asyncio.sleep(0.1)
        
        logger.error("⏰ Image capture timeout")
        self._reset_image_state()
        return None
    
    async def start_streaming(self, 
                            callback: Callable[[ImageFrame], None],
                            interval: float = 0.5,
                            quality: int = 25) -> bool:
        """Start continuous frame streaming"""
        if not self.connected:
            logger.error("Not connected to device")
            return False
        
        logger.info(f"📹 Starting frame streaming (interval: {interval}s, quality: {quality})")
        
        self.image_callback = callback
        self.is_streaming = True
        
        # Set parameters
        await self.send_command(f"INTERVAL:{interval:.3f}")
        await self.send_command(f"QUALITY:{quality}")
        
        # Start streaming
        return await self.send_command("START_FRAMES")
    
    async def stop_streaming(self) -> bool:
        """Stop frame streaming"""
        logger.info("⏹️ Stopping frame streaming")
        self.is_streaming = False
        self.image_callback = None
        return await self.send_command("STOP_FRAMES")
    
    async def set_quality(self, quality: int) -> bool:
        """Set image quality (4-63)"""
        quality = max(4, min(63, quality))
        return await self.send_command(f"QUALITY:{quality}")
    
    async def set_resolution(self, size_code: int) -> bool:
        """Set camera resolution
        
        Args:
            size_code: 1=QQVGA(160x120), 5=QVGA(320x240), 6=CIF(400x296), 
                      7=HVGA(480x320), 8=VGA(640x480)
        """
        return await self.send_command(f"SIZE:{size_code}")
    
    async def set_interval(self, interval: float) -> bool:
        """Set frame interval in seconds"""
        return await self.send_command(f"INTERVAL:{interval:.3f}")
    
    def _handle_status_data(self, characteristic: BleakGATTCharacteristic, data: bytearray):
        """Handle status data from ESP32S3"""
        try:
            status_json = data.decode('utf-8')
            logger.debug(f"Status: {status_json}")
            
            if self.status_callback:
                import json
                status_dict = json.loads(status_json)
                self.status_callback(status_dict)
                
        except Exception as e:
            logger.error(f"Failed to parse status: {e}")
    
    def _handle_image_data(self, characteristic: BleakGATTCharacteristic, data: bytearray):
        """Handle image data (single captures)"""
        self._handle_image_reception(data, is_frame=False)
    
    def _handle_frame_data(self, characteristic: BleakGATTCharacteristic, data: bytearray):
        """Handle frame data (streaming)"""
        self._handle_image_reception(data, is_frame=True)
    
    def _handle_image_reception(self, data: bytearray, is_frame: bool):
        """Handle incoming image/frame data"""
        if len(data) == 0:
            return
        
        # Track performance
        self.performance_stats['bytes_received'] += len(data)
        
        data_type = data[0]
        
        if data_type == 0x01:  # Start header
            # ESP32 header: [type][chunks_hi][chunks_lo][size_b0][size_b1][size_b2][size_b3]
            chunks = (data[1] << 8) | data[2]
            size = data[3] | (data[4] << 8) | (data[5] << 16) | (data[6] << 24)  # Little-endian
            
            self.expected_chunks = chunks
            self.expected_size = size
            self.received_chunks = 0
            self.image_buffer = bytearray(size)
            
            logger.debug(f"📋 Starting {'frame' if is_frame else 'image'}: {size} bytes ({chunks} chunks)")
            
        elif data_type == 0x02:  # Data chunk
            if self.image_buffer is None:
                return
            
            chunk_index = (data[1] << 8) | data[2]
            chunk_data = data[3:]
            
            # Calculate offset based on 510-byte chunks (ESP32 optimization)
            offset = chunk_index * MAX_CHUNK_SIZE
            
            if offset + len(chunk_data) <= len(self.image_buffer):
                self.image_buffer[offset:offset + len(chunk_data)] = chunk_data
                self.received_chunks += 1
                
                # Auto-process when all chunks received
                if self.received_chunks == self.expected_chunks:
                    self._process_complete_image(is_frame)
            
        elif data_type == 0x03:  # End marker
            # Process image if we have sufficient data (95% threshold)
            if self.image_buffer and self.received_chunks >= self.expected_chunks * 0.95:
                self._process_complete_image(is_frame)
    
    def _process_complete_image(self, is_frame: bool):
        """Process a complete image"""
        if not self.image_buffer:
            return
        
        # Create image frame
        image_data = bytes(self.image_buffer[:self.expected_size])
        self.current_frame_number += 1
        
        frame = ImageFrame(
            data=image_data,
            size=len(image_data),
            chunks_received=self.received_chunks,
            chunks_expected=self.expected_chunks,
            timestamp=time.time(),
            frame_number=self.current_frame_number
        )
        
        # Update performance stats
        self.performance_stats['frames_received'] += 1
        self.performance_stats['last_frame_time'] = time.time()
        
        logger.debug(f"✅ {'Frame' if is_frame else 'Image'} #{self.current_frame_number}: "
                    f"{len(image_data)} bytes ({frame.completion_rate:.1f}%)")
        
        # Call callback for streaming
        if is_frame and self.is_streaming and self.image_callback:
            try:
                self.image_callback(frame)
            except Exception as e:
                logger.error(f"Error in image callback: {e}")
        
        self._reset_image_state()
    
    def _reset_image_state(self):
        """Reset image reception state"""
        self.image_buffer = None
        self.expected_chunks = 0
        self.expected_size = 0
        self.received_chunks = 0
    
    def get_performance_stats(self) -> Dict[str, Any]:
        """Get performance statistics"""
        stats = self.performance_stats.copy()
        
        if stats['start_time']:
            elapsed = time.time() - stats['start_time']
            stats['elapsed_time'] = elapsed
            stats['avg_fps'] = stats['frames_received'] / elapsed if elapsed > 0 else 0
            stats['avg_kbps'] = (stats['bytes_received'] * 8) / (elapsed * 1000) if elapsed > 0 else 0
        
        return stats
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.connected:
            asyncio.create_task(self.disconnect())


# Example usage and test functions
async def main():
    """Example usage of ESP32Camera"""
    camera = ESP32Camera()
    
    try:
        # Connect to camera
        if not await camera.connect():
            print("Failed to connect to camera")
            return
        
        # Capture a single image
        print("\n📷 Capturing single image...")
        image = await camera.capture_image()
        if image:
            print(f"Image captured: {image.size} bytes")
            image.save("captured_image.jpg")
            print("Image saved as captured_image.jpg")
            
            # Convert to PIL Image for processing
            pil_image = image.to_pil_image()
            print(f"PIL Image size: {pil_image.size}")
        
        # Start streaming for 10 seconds
        print("\n📹 Starting streaming for 10 seconds...")
        
        def frame_callback(frame: ImageFrame):
            print(f"Frame {frame.frame_number}: {frame.size} bytes ({frame.completion_rate:.1f}%)")
            frame.save(f"frame_{frame.frame_number:04d}.jpg")
        
        await camera.start_streaming(frame_callback, interval=0.5, quality=25)
        await asyncio.sleep(10)
        await camera.stop_streaming()
        
        # Show performance stats
        stats = camera.get_performance_stats()
        print(f"\n📊 Performance Stats:")
        print(f"  Frames received: {stats['frames_received']}")
        print(f"  Average FPS: {stats.get('avg_fps', 0):.1f}")
        print(f"  Average bandwidth: {stats.get('avg_kbps', 0):.1f} kbps")
        
    finally:
        await camera.disconnect()


if __name__ == "__main__":
    asyncio.run(main()) 