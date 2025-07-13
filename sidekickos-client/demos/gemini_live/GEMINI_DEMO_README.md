# SidekickOS + Gemini Live Demo

A smart glasses demo that connects your SidekickOS BLE camera to Google's Gemini Live API for real-time computer vision and audio interaction.

## 🎯 **What This Demo Does**

- **Captures images** from your SidekickOS BLE camera
- **Streams audio** from your microphone
- **Sends both** to Google's Gemini Live API
- **Receives audio responses** from Gemini that describe what it sees
- **Creates a smart glasses experience** where AI can see and respond to your world

## 🚀 **Quick Start**

### **1. Install Dependencies**
```bash
pip install -r requirements_gemini.txt
```

### **2. Get Gemini API Key**
1. Go to [Google AI Studio](https://aistudio.google.com/app/apikey)
2. Create a new API key
3. Set it as an environment variable:
```bash
export GEMINI_API_KEY="your-api-key-here"
```

### **3. Prepare Your Hardware**
- Make sure your SidekickOS device is powered on
- Ensure it's in pairing/discoverable mode
- Have your microphone and speakers ready

### **4. Run the Demo**
```bash
cd demos/gemini_live
python sidekickos_gemini_demo.py
```

## 💡 **How to Use**

1. **Start the demo** - It will automatically connect to your SidekickOS camera
2. **Speak naturally** - Gemini will hear your voice and respond with audio
3. **Point your camera** - Gemini can see what your camera sees
4. **Ask questions** like:
   - "What do you see?"
   - "Describe what's in front of me"
   - "What color is this object?"
   - "Read this text to me"
5. **Type messages** in the console if needed
6. **Type 'q'** to quit

## 🎬 **Example Interactions**

- **You**: "What do you see?"
- **Gemini**: "I can see a cluttered desk with a laptop, some papers, and a coffee mug. The lighting appears to be natural daylight coming from the left."

- **You**: "What color is my shirt?"
- **Gemini**: "I can see you're wearing a blue shirt."

- **You**: "Read this text to me"
- **Gemini**: "I can see some text that says 'Hello World' on what appears to be a computer screen."

## 🔧 **Technical Details**

### **Camera Settings**
- **Resolution**: QVGA (320x240) for optimal performance
- **Quality**: 30 (good balance of quality and speed)
- **Capture Rate**: Every 2 seconds to avoid overwhelming the API

### **Audio Settings**
- **Input**: 16kHz mono PCM
- **Output**: 24kHz mono PCM
- **Voice**: Zephyr (configurable)

### **Gemini Configuration**
- **Model**: `gemini-2.5-flash-preview-native-audio-dialog`
- **Response**: Audio only (optimized for smart glasses)
- **Context Window**: 25.6K tokens with sliding window compression

## 🐛 **Troubleshooting**

### **Camera Connection Issues**
- Ensure your SidekickOS device is powered on and nearby
- Check that Bluetooth is enabled on your computer
- Try restarting the SidekickOS device

### **Audio Issues**
- Make sure your microphone permissions are granted
- Check your default audio devices
- Ensure speakers/headphones are connected

### **API Issues**
- Verify your `GEMINI_API_KEY` is set correctly
- Check your internet connection
- Ensure you have API quota available

### **Performance Issues**
- The demo captures frames every 2 seconds to balance performance
- You can adjust the frame rate in the code if needed
- Consider using a wired connection for better stability

## 🎨 **Customization**

You can modify the demo by:
- Changing the camera resolution: `await self.camera.set_resolution(1-8)`
- Adjusting frame capture rate: Change `await asyncio.sleep(2.0)` in `get_frames()`
- Modifying the AI voice: Change `voice_name` in the config
- Adding custom system prompts: Edit the `initial_context` message

## 📝 **Notes**

- This demo requires a stable internet connection
- Gemini Live API usage counts against your API quota
- The camera captures are optimized for real-time processing
- Audio responses are generated in real-time by Gemini

## 🚨 **Privacy Notice**

This demo sends camera images and audio to Google's Gemini Live API. Make sure you're comfortable with this before running the demo. Images and audio are processed by Google's servers.

---

**Enjoy your smart glasses experience with SidekickOS + Gemini Live!** 🤖👓 