#include "esp_camera.h"
#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEUtils.h"
#include "BLE2902.h"
#include "driver/i2s.h"
#include "esp_timer.h"
#include "SPIFFS.h"
#include "esp_task_wdt.h"

// Define camera model to use correct pins
#define CAMERA_MODEL_XIAO_ESP32S3

// Camera pin definitions for XIAO ESP32S3 Sense (from camera_pins.h)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

#define LED_GPIO_NUM      21

// Microphone I2S pins
#define I2S_WS_PIN         42
#define I2S_SD_PIN         41
#define I2S_SCK_PIN        1

// BLE Configuration
#define BLE_SERVICE_UUID           "12345678-1234-1234-1234-123456789abc"
#define CONTROL_CHAR_UUID          "87654321-4321-4321-4321-cba987654321"
#define STATUS_CHAR_UUID           "11111111-2222-3333-4444-555555555555"
#define IMAGE_CHAR_UUID            "22222222-3333-4444-5555-666666666666"
#define FRAME_CONTROL_CHAR_UUID    "44444444-5555-6666-7777-888888888888"
#define AUDIO_CHAR_UUID            "33333333-4444-5555-6666-777777777777"

// Global variables
BLEServer* pBLEServer = nullptr;
BLECharacteristic* pControlCharacteristic = nullptr;
BLECharacteristic* pStatusCharacteristic = nullptr;
BLECharacteristic* pImageCharacteristic = nullptr;
BLECharacteristic* pFrameControlCharacteristic = nullptr;
BLECharacteristic* pAudioCharacteristic = nullptr;

bool bleDeviceConnected = false;
bool frameStreamingEnabled = false;
bool audioStreamingEnabled = false;
float frameInterval = 1.0f; // seconds between frames (was int)
int imageQuality = 25; // JPEG quality (higher = lower quality)
framesize_t currentFrameSize = FRAMESIZE_QQVGA; // 160x120 for BLE

// Frame streaming variables
unsigned long lastFrameTime = 0;
bool sendingFrame = false;

// Audio variables
const int AUDIO_BUFFER_SIZE = 256;
int16_t audioBuffer[AUDIO_BUFFER_SIZE];
unsigned long lastAudioTime = 0;
const int AUDIO_INTERVAL = 20; // ms between audio packets

// Configuration structure
struct DeviceConfig {
  float frameInterval;
  int imageQuality;
  framesize_t frameSize;
  int audioSampleRate;
  bool motionDetection;
} config;

// Forward declarations
void sendBLEStatus();
void handleControlCommand(String command);
void setFrameSize(int size);
void sendFrame();
void captureHighResImage();
void sendTestImage(); // Added for test pattern
void sendImageViaBLE(uint8_t* imageData, size_t imageSize, BLECharacteristic* characteristic, bool isFrame);
void streamAudio();
void loadConfig();
void saveConfig();
void initCamera();
void initMicrophone();
void initBLE();

// Add cleanup function declaration
void cleanupOnDisconnect();

class MyBLEServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    bleDeviceConnected = true;
    Serial.println("BLE Client connected");
    sendBLEStatus();
  }

  void onDisconnect(BLEServer* pServer) {
    Serial.println("BLE Client disconnected");
    
    // Comprehensive cleanup on disconnect
    cleanupOnDisconnect();
    
    // Restart advertising after cleanup
    pServer->startAdvertising();
    Serial.println("Advertising restarted after disconnect");
  }
};

class ControlCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = pCharacteristic->getValue().c_str();
    if (value.length() > 0) {
      handleControlCommand(value);
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32S3 BLE Camera & Audio Streamer Starting...");
  Serial.println("======================================");

  // Initialize ESP32 task watchdog (5s timeout)
  esp_task_wdt_config_t twdt_config = {
    .timeout_ms = 5000, // 5 seconds
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, // All cores
    .trigger_panic = true
  };
  esp_task_wdt_init(&twdt_config);
  esp_task_wdt_add(NULL);

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
  }

  // Initialize camera
  Serial.println("Initializing camera...");
  initCamera();
  
  // Initialize microphone
  Serial.println("Initializing microphone...");
  initMicrophone();
  
  // Initialize BLE
  Serial.println("Initializing BLE...");
  initBLE();
  
  // Load configuration
  loadConfig();
  
  Serial.println("======================================");
  Serial.println("System initialized successfully!");
  Serial.println("Device ready for BLE connections...");
  Serial.println("Device name: ESP32S3-Camera");
  Serial.println("Look for this device in your browser's BLE scan");
  Serial.println("======================================");
}

void loop() {
  esp_task_wdt_reset(); // Reset watchdog each loop
  // Print periodic status for debugging
  static unsigned long lastStatusPrint = 0;
  if (millis() - lastStatusPrint > 10000) { // Every 10 seconds
    Serial.printf("Status: BLE %s, Frames %s, Audio %s, Free heap: %d bytes\n", 
                  bleDeviceConnected ? "connected" : "advertising",
                  frameStreamingEnabled ? "streaming" : "stopped",
                  audioStreamingEnabled ? "streaming" : "stopped",
                  ESP.getFreeHeap());
    lastStatusPrint = millis();
  }
  
  // Handle frame streaming
  if (frameStreamingEnabled && bleDeviceConnected && !sendingFrame) {
    unsigned long currentTime = millis();
    if (currentTime - lastFrameTime >= frameInterval * 1000.0f) {
      sendFrame();
      lastFrameTime = currentTime;
    }
  }
  
  // Handle audio streaming
  if (audioStreamingEnabled && bleDeviceConnected) {
    unsigned long currentTime = millis();
    if (currentTime - lastAudioTime >= AUDIO_INTERVAL) {
      streamAudio();
      lastAudioTime = currentTime;
    }
  }
  
  delay(10); // Small delay to prevent watchdog issues (restored from 1)
}

void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Use exact same configuration as working example
  config.frame_size = FRAMESIZE_SVGA;  // Start with SVGA like the working example
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;  // Use same grab mode
  config.fb_location = CAMERA_FB_IN_PSRAM;  // Explicit PSRAM usage
  config.jpeg_quality = 12;  // Same quality as working example
  config.fb_count = 1;  // Single buffer like working example

  Serial.printf("Available PSRAM: %d bytes\n", ESP.getPsramSize());
  Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    Serial.println("Trying smaller frame size...");
    
    // Fallback to smaller frame size
    config.frame_size = FRAMESIZE_QVGA;  // 320x240
    config.jpeg_quality = 20;
    
    err = esp_camera_init(&config);
    if (err != ESP_OK) {
      Serial.printf("Camera fallback init also failed with error 0x%x\n", err);
      Serial.println("Trying minimal configuration...");
      
      // Final fallback - minimal config
      config.frame_size = FRAMESIZE_QQVGA;  // 160x120
      config.jpeg_quality = 30;
      
      err = esp_camera_init(&config);
      if (err != ESP_OK) {
        Serial.printf("Camera minimal init failed with error 0x%x\n", err);
        Serial.println("Camera not available - continuing without camera");
        return;
      }
    }
  }
  
  // Set initial camera settings optimized for BLE
  sensor_t* s = esp_camera_sensor_get();
  if (s != NULL) {
    // Drop down frame size for BLE transmission
    s->set_framesize(s, FRAMESIZE_QQVGA); // 160x120 for BLE
    s->set_quality(s, imageQuality);
    s->set_brightness(s, 0);     // -2 to 2
    s->set_contrast(s, 0);       // -2 to 2  
    s->set_saturation(s, 0);     // -2 to 2
    s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
    s->set_colorbar(s, 0);       // 0 = disable, 1 = enable
    s->set_whitebal(s, 1);       // 0 = disable, 1 = enable
    s->set_gain_ctrl(s, 1);      // 0 = disable, 1 = enable
    s->set_exposure_ctrl(s, 1);  // 0 = disable, 1 = enable
    s->set_hmirror(s, 0);        // 0 = disable, 1 = enable
    s->set_vflip(s, 0);          // 0 = disable, 1 = enable
    
    Serial.println("Camera initialized successfully for BLE streaming");
    currentFrameSize = FRAMESIZE_QQVGA;
  } else {
    Serial.println("Failed to get camera sensor");
  }
}

void initMicrophone() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
    .sample_rate = 8000, // Lower sample rate for BLE
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD_PIN
  };

  esp_err_t result = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  if (result != ESP_OK) {
    Serial.println("Failed to install I2S driver");
    return;
  }

  result = i2s_set_pin(I2S_NUM_0, &pin_config);
  if (result != ESP_OK) {
    Serial.println("Failed to set I2S pins");
    return;
  }
  
  Serial.println("Microphone initialized for BLE audio");
}

void initBLE() {
  BLEDevice::init("ESP32S3-Camera");
  pBLEServer = BLEDevice::createServer();
  pBLEServer->setCallbacks(new MyBLEServerCallbacks());

  BLEService *pService = pBLEServer->createService(BLE_SERVICE_UUID);

  // Control characteristic
  pControlCharacteristic = pService->createCharacteristic(
    CONTROL_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pControlCharacteristic->setCallbacks(new ControlCallbacks());

  // Status characteristic
  pStatusCharacteristic = pService->createCharacteristic(
    STATUS_CHAR_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pStatusCharacteristic->addDescriptor(new BLE2902());

  // Image characteristic for single captures
  pImageCharacteristic = pService->createCharacteristic(
    IMAGE_CHAR_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pImageCharacteristic->addDescriptor(new BLE2902());

  // Frame control characteristic for streaming frames
  pFrameControlCharacteristic = pService->createCharacteristic(
    FRAME_CONTROL_CHAR_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pFrameControlCharacteristic->addDescriptor(new BLE2902());

  // Audio characteristic for audio streaming
  pAudioCharacteristic = pService->createCharacteristic(
    AUDIO_CHAR_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pAudioCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  // Configure advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // Functions to help with iPhone connection
  pAdvertising->setMinPreferred(0x12);
  
  // Start advertising
  pBLEServer->startAdvertising();

  Serial.println("BLE server started and advertising...");
  Serial.println("Device name: ESP32S3-Camera");
  Serial.println("Service UUID: " + String(BLE_SERVICE_UUID));
}

void handleControlCommand(String command) {
  Serial.println("Received command: " + command);
  
  if (command == "CAPTURE") {
    captureHighResImage();
  }
  else if (command == "START_FRAMES") {
    frameStreamingEnabled = true;
    lastFrameTime = 0; // Force immediate first frame
    sendBLEStatus();
    Serial.println("Frame streaming started");
  }
  else if (command == "STOP_FRAMES") {
    frameStreamingEnabled = false;
    sendBLEStatus();
    Serial.println("Frame streaming stopped");
  }
  else if (command == "START_AUDIO") {
    audioStreamingEnabled = true;
    sendBLEStatus();
    Serial.println("Audio streaming started");
  }
  else if (command == "STOP_AUDIO") {
    audioStreamingEnabled = false;
    sendBLEStatus();
    Serial.println("Audio streaming stopped");
  }
  else if (command.indexOf("INTERVAL:") == 0) {
    frameInterval = command.substring(9).toFloat();
    frameInterval = max(0.1f, min(60.0f, frameInterval)); // Limit between 0.1-60 seconds
    sendBLEStatus();
    Serial.printf("Frame interval set to %.2f seconds\n", frameInterval);
  }
  else if (command.indexOf("QUALITY:") == 0) {
    imageQuality = command.substring(8).toInt();
    imageQuality = max(4, min(63, imageQuality)); // Limit quality range
    sensor_t* s = esp_camera_sensor_get();
    s->set_quality(s, imageQuality);
    Serial.printf("Image quality set to %d\n", imageQuality);
  }
  else if (command.indexOf("SIZE:") == 0) {
    int size = command.substring(5).toInt();
    setFrameSize(size);
  }
  else if (command == "STATUS") {
    sendBLEStatus();
  }
  
  saveConfig();
}

void setFrameSize(int size) {
  sensor_t* s = esp_camera_sensor_get();
  if (s == NULL) {
    Serial.println("Camera not available");
    return;
  }
  
  framesize_t newSize;
  switch(size) {
    case 0: newSize = FRAMESIZE_96X96; break;   // 96x96 - smallest
    case 1: newSize = FRAMESIZE_QQVGA; break;   // 160x120 
    case 2: newSize = FRAMESIZE_QVGA; break;    // 320x240
    case 3: newSize = FRAMESIZE_CIF; break;     // 352x288
    case 4: newSize = FRAMESIZE_VGA; break;     // 640x480 (may fail with low memory)
    default: newSize = FRAMESIZE_QQVGA; break;
  }
  
  s->set_framesize(s, newSize);
  currentFrameSize = newSize;
  
  Serial.printf("Frame size changed to %dx%d\n", 
    (newSize == FRAMESIZE_96X96) ? 96 : 
    (newSize == FRAMESIZE_QQVGA) ? 160 : 
    (newSize == FRAMESIZE_QVGA) ? 320 : 
    (newSize == FRAMESIZE_CIF) ? 352 : 640,
    (newSize == FRAMESIZE_96X96) ? 96 :
    (newSize == FRAMESIZE_QQVGA) ? 120 : 
    (newSize == FRAMESIZE_QVGA) ? 240 : 
    (newSize == FRAMESIZE_CIF) ? 288 : 480);
}

void sendFrame() {
  if (!bleDeviceConnected || sendingFrame) return;
  
  sensor_t* s = esp_camera_sensor_get();
  if (s == NULL) {
    // Camera not available - send a test pattern instead
    Serial.println("Camera not available - sending test pattern");
    sendTestImage();
    return;
  }
  
  sendingFrame = true;
  
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed - sending test pattern");
    sendTestImage();
    sendingFrame = false;
    return;
  }
  
  Serial.printf("Sending frame: %d bytes (%dx%d)\n", fb->len, fb->width, fb->height);
  
  // Send frame via BLE in chunks
  sendImageViaBLE(fb->buf, fb->len, pFrameControlCharacteristic, true);
  
  esp_camera_fb_return(fb);
  sendingFrame = false;
}

void captureHighResImage() {
  if (sendingFrame) return;
  
  sensor_t* s = esp_camera_sensor_get();
  if (s == NULL) {
    Serial.println("Camera not available for high-res capture - sending test pattern");
    sendTestImage();
    return;
  }
  
  // Temporarily switch to higher resolution for single capture
  framesize_t originalSize = currentFrameSize;
  
  // Only try higher resolution if we have enough memory
  if (ESP.getFreeHeap() > 100000) {
    s->set_framesize(s, FRAMESIZE_QVGA); // 320x240 for single shots
    s->set_quality(s, 12); // Higher quality for single shots
  } else {
    Serial.println("Low memory - using current frame size for capture");
  }
  
  delay(100); // Let camera adjust
  
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("High-res capture failed - sending test pattern");
    sendTestImage();
    // Restore original settings
    s->set_framesize(s, originalSize);
    s->set_quality(s, imageQuality);
    return;
  }
  
  Serial.printf("Captured image: %d bytes (%dx%d)\n", fb->len, fb->width, fb->height);
  
  // Send via BLE
  sendImageViaBLE(fb->buf, fb->len, pImageCharacteristic, false);
  
  esp_camera_fb_return(fb);
  
  // Restore original settings
  s->set_framesize(s, originalSize);
  s->set_quality(s, imageQuality);
}

void sendTestImage() {
  // Create a simple test pattern when camera is not available
  // This creates a minimal JPEG header + some test data
  
  uint8_t testImageData[] = {
    // Minimal JPEG header for a small test image
    0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xFF, 0xC0, 0x00, 0x11,
    0x08, 0x00, 0x20, 0x00, 0x20, 0x01, 0x01, 0x11, 0x00, 0x02, 0x11, 0x01,
    0x03, 0x11, 0x01, 0xFF, 0xC4, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x08, 0xFF, 0xC4, 0x00, 0x14, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,
    0xDA, 0x00, 0x0C, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3F,
    0x00, 0xAA, 0xFF, 0xD9
  };
  
  size_t testImageSize = sizeof(testImageData);
  
  Serial.printf("Sending test pattern: %d bytes\n", testImageSize);
  
  // Send test image via BLE
  sendImageViaBLE(testImageData, testImageSize, pFrameControlCharacteristic, true);
}

void sendImageViaBLE(uint8_t* imageData, size_t imageSize, BLECharacteristic* characteristic, bool isFrame) {
  if (!bleDeviceConnected || !characteristic) return;
  
  const int maxChunkSize = 490; // Conservative BLE chunk size (512 - 22 BLE overhead)
  
  // Send header with total size and type
  uint8_t header[6] = {
    0x01, // Start marker
    (uint8_t)(imageSize >> 24), 
    (uint8_t)(imageSize >> 16), 
    (uint8_t)(imageSize >> 8), 
    (uint8_t)imageSize,
    isFrame ? 0x02 : 0x01 // Frame vs single image marker
  };
  
  characteristic->setValue(header, 6);
  characteristic->notify();
  delay(5); // Reduced from 20 for faster BLE chunking
  
  // Send image data in chunks
  size_t bytesSent = 0;
  uint16_t chunkIndex = 0;
  
  while (bytesSent < imageSize) {
    size_t chunkSize = (size_t)min((int)(maxChunkSize - 3), (int)(imageSize - bytesSent)); // Reserve 3 bytes for header
    
    uint8_t chunk[maxChunkSize];
    chunk[0] = 0x02; // Data chunk marker
    chunk[1] = (uint8_t)(chunkIndex >> 8);
    chunk[2] = (uint8_t)chunkIndex;
    
    memcpy(chunk + 3, imageData + bytesSent, chunkSize);
    
    characteristic->setValue(chunk, chunkSize + 3);
    characteristic->notify();
    
    bytesSent += chunkSize;
    chunkIndex++;
    
    delay(5); // Reduced from 20 for faster BLE chunking
    
    // Watchdog reset prevention
    if (chunkIndex % 20 == 0) {
      delay(5); // Reduced from 20 for faster BLE chunking
    }
  }
  
  // Send completion marker
  uint8_t complete[3] = {0x03, (uint8_t)(chunkIndex >> 8), (uint8_t)chunkIndex};
  characteristic->setValue(complete, 3);
  characteristic->notify();
  
  Serial.printf("Image sent: %d bytes in %d chunks\n", imageSize, chunkIndex);
}

void streamAudio() {
  if (!bleDeviceConnected || !pAudioCharacteristic) return;
  
  size_t bytesRead = 0;
  esp_err_t result = i2s_read(I2S_NUM_0, audioBuffer, sizeof(audioBuffer), &bytesRead, 0);
  
  if (result == ESP_OK && bytesRead > 0) {
    // Apply simple audio processing
    for (int i = 0; i < bytesRead / 2; i++) {
      // Noise gate
      if (abs(audioBuffer[i]) < 50) {
        audioBuffer[i] = 0;
      }
      // Simple gain control
      audioBuffer[i] = (int16_t)(audioBuffer[i] * 2.0);
    }
    
    // Send audio data via BLE with header
    uint8_t audioPacket[500];
    audioPacket[0] = 0xAA; // Audio packet marker
    audioPacket[1] = 0x55;
    audioPacket[2] = (uint8_t)(bytesRead >> 8);
    audioPacket[3] = (uint8_t)bytesRead;
    
    size_t packetSize = min(bytesRead, (size_t)496); // Leave room for 4-byte header
    memcpy(audioPacket + 4, audioBuffer, packetSize);
    
    pAudioCharacteristic->setValue(audioPacket, packetSize + 4);
    pAudioCharacteristic->notify();
  }
}

void sendBLEStatus() {
  if (!bleDeviceConnected || !pStatusCharacteristic) return;
  
  String status = "{";
  status += "\"ble\":" + String(bleDeviceConnected ? "true" : "false") + ",";
  status += "\"frames\":" + String(frameStreamingEnabled ? "true" : "false") + ",";
  status += "\"audio\":" + String(audioStreamingEnabled ? "true" : "false") + ",";
  status += "\"interval\":" + String(frameInterval) + ",";
  status += "\"quality\":" + String(imageQuality) + ",";
  status += "\"size\":" + String(currentFrameSize) + ",";
  status += "\"battery\":" + String(analogRead(A0) * 100 / 4095) + ",";
  status += "\"free_heap\":" + String(ESP.getFreeHeap()) + "";
  status += "}";
  
  pStatusCharacteristic->setValue(status.c_str());
  pStatusCharacteristic->notify();
  
  Serial.println("BLE Status sent: " + status);
}

void loadConfig() {
  File file = SPIFFS.open("/config.txt", "r");
  if (file) {
    config.frameInterval = file.readStringUntil('\n').toFloat();
    config.imageQuality = file.readStringUntil('\n').toInt();
    config.frameSize = (framesize_t)file.readStringUntil('\n').toInt();
    config.audioSampleRate = file.readStringUntil('\n').toInt();
    config.motionDetection = file.readStringUntil('\n').toInt();
    file.close();
    
    // Apply loaded configuration
    frameInterval = config.frameInterval > 0.0f ? config.frameInterval : 1.0f;
    imageQuality = config.imageQuality > 0 ? config.imageQuality : 25;
    currentFrameSize = config.frameSize;
    
    Serial.println("Configuration loaded");
  } else {
    // Default configuration
    config.frameInterval = 1.0f;
    config.imageQuality = 25;
    config.frameSize = FRAMESIZE_QVGA;
    config.audioSampleRate = 8000;
    config.motionDetection = false;
    
    frameInterval = config.frameInterval;
    imageQuality = config.imageQuality;
    currentFrameSize = config.frameSize;
    
    Serial.println("Using default configuration");
  }
}

void saveConfig() {
  config.frameInterval = frameInterval;
  config.imageQuality = imageQuality;
  config.frameSize = currentFrameSize;
  
  File file = SPIFFS.open("/config.txt", "w");
  if (file) {
    file.println(config.frameInterval, 4);
    file.println(config.imageQuality);
    file.println(config.frameSize);
    file.println(config.audioSampleRate);
    file.println(config.motionDetection);
    file.close();
    Serial.println("Configuration saved");
  }
}

// Add the cleanup function before the BLE callbacks
void cleanupOnDisconnect() {
  Serial.println("Cleaning up resources on disconnect...");
  
  // Reset connection state
  bleDeviceConnected = false;
  
  // Stop all streaming activities
  frameStreamingEnabled = false;
  audioStreamingEnabled = false;
  
  // Reset frame capture flag
  sendingFrame = false;
  
  // Reset frame interval to default (conservative on disconnect)
  frameInterval = 1.0f;  // 1 second default
  
  // Reset image quality to default
  imageQuality = 25;
  
  // Reset frame size to safe default
  currentFrameSize = FRAMESIZE_QVGA;  // Safe default size
  
  // Reset camera settings to safe defaults
  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    s->set_quality(s, imageQuality);
    s->set_framesize(s, currentFrameSize);
  }
  
  // Give system time to process cleanup
  delay(50);
  
  Serial.println("Disconnect cleanup completed");
}