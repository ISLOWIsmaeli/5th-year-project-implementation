#include <driver/i2s.h>
#include <math.h>
#include <SD.h>
#include <SPI.h>

// I2S pins
#define I2S_SCK 14
#define I2S_WS 15
#define I2S_SD 32  // Mic data
#define I2S_DOUT 25 // Amp data

// SD Card pins (adjust based on your wiring)
#define SD_CS 5
#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK 18

// Recording parameters
#define SAMPLE_RATE 44100
#define RECORD_TIME 30  // seconds
#define BUFFER_SIZE 512
#define TOTAL_SAMPLES (SAMPLE_RATE * RECORD_TIME)

// File name for recorded audio
const char* filename = "/recording.wav";

// State variables
enum State {
  RECORDING,
  WAITING,
  PLAYING,
  IDLE
};

State currentState = RECORDING;
unsigned long stateStartTime = 0;
File audioFile;
int32_t samplesRecorded = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Audio Recording System...");
  
  // Initialize SD card
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card initialization failed!");
    return;
  }
  Serial.println("SD Card initialized successfully");
  
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
  
  // Pin configuration
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_SD
  };
  i2s_set_pin(I2S_NUM_0, &pin_config);
  
  // Start recording immediately
  startRecording();
  stateStartTime = millis();
  Serial.println("Recording started...");
}

void loop() {
  switch(currentState) {
    case RECORDING:
      handleRecording();
      break;
    case WAITING:
      handleWaiting();
      break;
    case PLAYING:
      handlePlayback();
      break;
    case IDLE:
      // Do nothing - process complete
      break;
  }
}

void startRecording() {
  // Remove existing file if it exists
  if (SD.exists(filename)) {
    SD.remove(filename);
  }
  
  // Create new file for recording
  audioFile = SD.open(filename, FILE_WRITE);
  if (!audioFile) {
    Serial.println("Failed to create recording file!");
    return;
  }
  
  // Write WAV header (we'll update it later with correct size)
  writeWAVHeader(audioFile, TOTAL_SAMPLES);
  samplesRecorded = 0;
}

void handleRecording() {
  int16_t raw_samples[BUFFER_SIZE];
  size_t bytes_read;
  
  // Read from microphone
  i2s_read(I2S_NUM_0, raw_samples, sizeof(raw_samples), &bytes_read, portMAX_DELAY);
  
  int samples_count = bytes_read / sizeof(int16_t);
  
  // Write samples to SD card
  if (audioFile && samplesRecorded < TOTAL_SAMPLES) {
    int samplesToWrite = min(samples_count, (int)(TOTAL_SAMPLES - samplesRecorded));
    audioFile.write((uint8_t*)raw_samples, samplesToWrite * sizeof(int16_t));
    samplesRecorded += samplesToWrite;
    
    // Print progress every 5 seconds
    if (samplesRecorded % (SAMPLE_RATE * 5) == 0) {
      Serial.print("Recording progress: ");
      Serial.print(samplesRecorded / SAMPLE_RATE);
      Serial.println(" seconds");
    }
  }
  
  // Check if recording time is complete
  if (samplesRecorded >= TOTAL_SAMPLES) {
    stopRecording();
    currentState = WAITING;
    stateStartTime = millis();
    Serial.println("Recording complete! Waiting 5 seconds...");
  }
}

void stopRecording() {
  if (audioFile) {
    // Update WAV header with actual recorded samples
    updateWAVHeader(audioFile, samplesRecorded);
    audioFile.close();
    Serial.print("Recording saved: ");
    Serial.print(samplesRecorded / SAMPLE_RATE);
    Serial.println(" seconds");
  }
}

void handleWaiting() {
  // Wait for 5 seconds
  if (millis() - stateStartTime >= 5000) {
    currentState = PLAYING;
    stateStartTime = millis();
    Serial.println("Starting playback...");
    startPlayback();
  }
}

void startPlayback() {
  audioFile = SD.open(filename, FILE_READ);
  if (!audioFile) {
    Serial.println("Failed to open recording file for playback!");
    currentState = IDLE;
    return;
  }
  
  // Skip WAV header (44 bytes)
  audioFile.seek(44);
}

void handlePlayback() {
  if (!audioFile || !audioFile.available()) {
    // Playback complete
    if (audioFile) {
      audioFile.close();
    }
    currentState = IDLE;
    Serial.println("Playback complete!");
    return;
  }
  
  int16_t playback_samples[BUFFER_SIZE];
  size_t bytes_to_read = sizeof(playback_samples);
  size_t bytes_available = audioFile.available();
  
  if (bytes_available < bytes_to_read) {
    bytes_to_read = bytes_available;
  }
  
  size_t bytes_read = audioFile.read((uint8_t*)playback_samples, bytes_to_read);
  
  if (bytes_read > 0) {
    int samples_count = bytes_read / sizeof(int16_t);
    
    // Apply volume boost for playback
    for(int i = 0; i < samples_count; i++) {
      int32_t boosted = playback_samples[i] * 3;
      playback_samples[i] = constrain(boosted, -32768, 32767);
    }
    
    // Send to amplifier
    size_t bytes_written;
    i2s_write(I2S_NUM_0, playback_samples, bytes_read, &bytes_written, portMAX_DELAY);
  }
}

void writeWAVHeader(File file, uint32_t sampleCount) {
  uint32_t fileSize = 44 + (sampleCount * 2) - 8;  // Total file size - 8
  uint32_t dataSize = sampleCount * 2;  // Data chunk size
  
  // RIFF header
  file.write((const uint8_t*)"RIFF", 4);
  file.write((uint8_t*)&fileSize, 4);
  file.write((const uint8_t*)"WAVE", 4);
  
  // Format chunk
  file.write((const uint8_t*)"fmt ", 4);
  uint32_t fmtSize = 16;
  file.write((uint8_t*)&fmtSize, 4);
  uint16_t audioFormat = 1;  // PCM
  file.write((uint8_t*)&audioFormat, 2);
  uint16_t numChannels = 1;  // Mono
  file.write((uint8_t*)&numChannels, 2);
  uint32_t sampleRate = SAMPLE_RATE;
  file.write((uint8_t*)&sampleRate, 4);
  uint32_t byteRate = SAMPLE_RATE * 2;  // Sample rate * channels * bytes per sample
  file.write((uint8_t*)&byteRate, 4);
  uint16_t blockAlign = 2;  // Channels * bytes per sample
  file.write((uint8_t*)&blockAlign, 2);
  uint16_t bitsPerSample = 16;
  file.write((uint8_t*)&bitsPerSample, 2);
  
  // Data chunk header
  file.write((const uint8_t*)"data", 4);
  file.write((uint8_t*)&dataSize, 4);
}

void updateWAVHeader(File file, uint32_t actualSampleCount) {
  uint32_t fileSize = 44 + (actualSampleCount * 2) - 8;
  uint32_t dataSize = actualSampleCount * 2;
  
  // Update file size in RIFF header
  file.seek(4);
  file.write((uint8_t*)&fileSize, 4);
  
  // Update data size in data chunk header
  file.seek(40);
  file.write((uint8_t*)&dataSize, 4);
}
