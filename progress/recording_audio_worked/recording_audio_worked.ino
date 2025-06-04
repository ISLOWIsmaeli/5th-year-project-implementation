#include <driver/i2s.h>
#include <SD.h>

// I2S pins for single microphone and amplifier
#define I2S_SCK 14
#define I2S_WS 15
#define I2S_SD_IN 32    // Microphone input
#define I2S_SD_OUT 22   // Amplifier output

// SD Card CS pin
#define SD_CS 5

// Button pin
#define BUTTON_PIN 4

#define SAMPLE_BUFFER_SIZE 64
#define BYTES_TO_READ (SAMPLE_BUFFER_SIZE * sizeof(int16_t))

// Simple variables
bool isRecording = false;
bool wasButtonPressed = false;
File recordingFile;
String filename = "/recording.raw";

void setup() {
  Serial.begin(115200);
  Serial.println("Single Mic Audio Recorder Starting...");
  
  // Button setup
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // SD Card setup
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card initialization failed!");
    while(1);
  }
  Serial.println("SD Card ready");
  
  // I2S configuration
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 4,
    .dma_buf_len = 64,
    .use_apll = false
  };
  
  // Install I2S driver
  if (i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL) != ESP_OK) {
    Serial.println("I2S driver install failed!");
    while(1);
  }
  
  // Set I2S pins
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_SD_OUT,
    .data_in_num = I2S_SD_IN
  };
  
  if (i2s_set_pin(I2S_NUM_0, &pin_config) != ESP_OK) {
    Serial.println("I2S pin setup failed!");
    while(1);
  }
  
  Serial.println("System ready!");
  Serial.println("Press and hold button to record, release to play");
}

void loop() {
  bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW);
  
  // Handle button press/release
  if (buttonPressed && !wasButtonPressed) {
    startRecording();
  }
  
  if (!buttonPressed && wasButtonPressed && isRecording) {
    stopRecording();
    playRecording();
  }
  
  wasButtonPressed = buttonPressed;
  
  // Process audio
  int16_t audioBuffer[SAMPLE_BUFFER_SIZE];
  size_t bytesRead;
  
  // Read audio from microphone
  if (i2s_read(I2S_NUM_0, audioBuffer, BYTES_TO_READ, &bytesRead, 10) == ESP_OK) {
    if (bytesRead > 0) {
      // If recording, save to SD card
      if (isRecording && recordingFile) {
        recordingFile.write((uint8_t*)audioBuffer, bytesRead);
      }
      
      // Always output audio for live monitoring
      size_t bytesWritten;
      i2s_write(I2S_NUM_0, audioBuffer, bytesRead, &bytesWritten, 10);
    }
  }
}

void startRecording() {
  Serial.println("🎤 Recording started...");
  isRecording = true;
  
  // Remove old recording if exists
  if (SD.exists(filename)) {
    SD.remove(filename);
  }
  
  // Create new recording file
  recordingFile = SD.open(filename, FILE_WRITE);
  if (!recordingFile) {
    Serial.println("❌ Failed to create recording file");
    isRecording = false;
  }
}

void stopRecording() {
  if (isRecording) {
    recordingFile.close();
    isRecording = false;
    Serial.println("⏹️ Recording stopped");
    
    // Show file size
    File tempFile = SD.open(filename);
    if (tempFile) {
      Serial.printf("📁 File size: %d bytes\n", tempFile.size());
      tempFile.close();
    }
  }
}

void playRecording() {
  File playFile = SD.open(filename);
  if (!playFile) {
    Serial.println("❌ No recording found to play");
    return;
  }
  
  Serial.println("🔊 Playing recording...");
  int16_t playBuffer[SAMPLE_BUFFER_SIZE];
  
  while (playFile.available()) {
    int bytesRead = playFile.read((uint8_t*)playBuffer, BYTES_TO_READ);
    if (bytesRead > 0) {
      size_t bytesWritten;
      i2s_write(I2S_NUM_0, playBuffer, bytesRead, &bytesWritten, 100);
    }
    
    // Check if button pressed to stop playback
    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("⏹️ Playback stopped by user");
      break;
    }
  }
  
  playFile.close();
  Serial.println("✅ Playback finished");
}
