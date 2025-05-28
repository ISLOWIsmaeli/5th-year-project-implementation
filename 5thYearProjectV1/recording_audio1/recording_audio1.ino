#include <driver/i2s.h>
#include <math.h>
#include <SD.h>
#include <SPI.h>

// I2S Pins
#define I2S_SCK 14
#define I2S_WS 15
#define I2S_SD 32   // Mic data
#define I2S_DOUT 25 // Amp data

// SD Card Pins
#define SD_CS 5     // Chip Select for SD card
#define SD_MOSI 23  // 
#define SD_MISO 19
#define SD_SCK 18   // You mentioned SCK as 23, but that conflicts with MISO - using 18 as an alternative

// Button pin
#define BUTTON_PIN 4  // Button for recording control

// Audio parameters
#define SAMPLE_RATE 44100
#define BUFFER_SIZE 512

// File to store recorded audio
File audioFile;
const char* recordingFileName = "/recording.raw";

// State control
bool isRecording = false;
bool buttonPressed = false;
unsigned long buttonPressTime = 0;

void setup() {
  Serial.begin(115200);
  
  // Initialize button pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize I2S
  initI2S();
  
  // Initialize SD card
  initSDCard();
}

void initI2S() {
  // I2S Configuration
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = BUFFER_SIZE,
    .use_apll = true
  };
  
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_SD
  };
  
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

void initSDCard() {
  // Configure SPI for SD card
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  
  // Initialize SD card
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card initialization failed!");
    while (1); // Stop if we can't access the SD card
  }
  Serial.println("SD Card initialized successfully");
  
  // Check if recording file exists and delete it
  if (SD.exists(recordingFileName)) {
    SD.remove(recordingFileName);
    Serial.println("Previous recording deleted");
  }
}

void startRecording() {
  // Open file for writing
  audioFile = SD.open(recordingFileName, FILE_WRITE);
  if (!audioFile) {
    Serial.println("Failed to open file for recording");
    return;
  }
  
  Serial.println("Recording started...");
  isRecording = true;
}

void stopRecording() {
  if (isRecording) {
    audioFile.close();
    Serial.println("Recording stopped");
    isRecording = false;
  }
}

void recordAudio() {
  int16_t samples[BUFFER_SIZE];
  size_t bytes_read;
  
  // Read audio from microphone
  i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytes_read, 0);
  
  if (bytes_read > 0) {
    // Write raw audio data to file
    audioFile.write((uint8_t*)samples, bytes_read);
  }
}

void playRecording() {
  // Open file for reading
  audioFile = SD.open(recordingFileName, FILE_READ);
  if (!audioFile) {
    Serial.println("Failed to open file for playback");
    return;
  }
  
  Serial.println("Playback started...");
  
  int16_t samples[BUFFER_SIZE];
  size_t bytes_written;
  
  // Read from file and play until end of file
  while (audioFile.available()) {
    // Read a buffer of audio data from the file
    int bytesRead = audioFile.read((uint8_t*)samples, sizeof(samples));
    
    if (bytesRead > 0) {
      // Apply volume boost
      for (int i = 0; i < bytesRead / 2; i++) {
        // Multiply sample by gain (3x = ~9.5dB boost)
        int32_t boosted = samples[i] * 3;
        
        // Prevent clipping
        samples[i] = constrain(boosted, -32768, 32767);
      }
      
      // Send to amplifier
      i2s_write(I2S_NUM_0, samples, bytesRead, &bytes_written, portMAX_DELAY);
    }
    
    // Check if button pressed again during playback
    if (digitalRead(BUTTON_PIN) == LOW) {
      audioFile.close();
      Serial.println("Playback interrupted");
      delay(500); // Debounce
      return;
    }
  }
  
  audioFile.close();
  Serial.println("Playback finished");
}

void loop() {
  // Check button state
  bool currentButtonState = (digitalRead(BUTTON_PIN) == LOW);
  
  // Button just pressed
  if (currentButtonState && !buttonPressed) {
    buttonPressed = true;
    buttonPressTime = millis();
    startRecording();
  }
  
  // Button still pressed - continue recording
  if (currentButtonState && buttonPressed && isRecording) {
    recordAudio();
  }
  
  // Button just released
  if (!currentButtonState && buttonPressed) {
    buttonPressed = false;
    
    // Stop recording and start playback if recording was in progress
    if (isRecording) {
      stopRecording();
      delay(500); // Small delay before playback starts
      playRecording();
    }
  }
}
