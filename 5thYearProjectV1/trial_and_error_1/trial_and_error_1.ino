#include <driver/i2s.h>
#include <SD.h>
#include <math.h>

// Primary I2S Bus (Mic1 + Amp)
#define PRIMARY_SCK 14
#define PRIMARY_WS 15
#define PRIMARY_SD 32
#define AMP_DOUT 22

// Secondary I2S Bus (Mic2)
#define SECONDARY_SCK 26
#define SECONDARY_WS 25
#define SECONDARY_SD 35

// SD Card CS pin
#define SD_CS 5

// Button pin
#define BUTTON_PIN 4

#define SAMPLE_BUFFER_SIZE 128
#define BYTES_TO_READ (SAMPLE_BUFFER_SIZE * sizeof(int16_t))

// Sensitivity and gain settings
#define MIC_GAIN 3.0          // Microphone input amplification (1.0-10.0)
#define OUTPUT_GAIN 2.5       // Speaker output amplification (1.0-5.0)
#define NOISE_GATE 50         // Minimum signal level to process (reduces background noise)

// LMS Adaptive Filter Parameters
#define FILTER_LENGTH 32      // Number of filter taps
#define LMS_MU 0.001          // LMS step size (learning rate)
#define RMS_WINDOW_SIZE 64    // Window size for RMS calculation
#define CONVERGENCE_THRESHOLD 0.01  // RMS error threshold for convergence detection

// Recording control variables
bool isRecording = false;
bool wasButtonPressed = false;
File recordingFile;
String filename = "/adaptive_filtered_recording.raw";

// LMS Adaptive Filter Variables
float filterCoeffs[FILTER_LENGTH] = {0};  // w(n) - filter coefficients
float inputBuffer[FILTER_LENGTH] = {0};   // x(n) - reference input buffer (mic2)
float errorBuffer[RMS_WINDOW_SIZE] = {0}; // For RMS error calculation
int errorIndex = 0;
bool hasConverged = false;
unsigned long lastRMSPrint = 0;
float lastRMSError = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Dual Microphone Recorder with LMS Adaptive Noise Cancellation");
  Serial.println("Mic1: Primary (desired signal + noise)");
  Serial.println("Mic2: Reference (noise source)");
  Serial.println("Filter Length: " + String(FILTER_LENGTH) + " taps");
  Serial.println("Learning Rate (μ): " + String(LMS_MU, 6));
  Serial.println("========================================");
  
  // Button setup
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // SD Card setup
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card initialization failed!");
    while(1);
  }
  Serial.println("SD Card ready");
  
  // Primary I2S configuration (Mic1 + Amplifier)
  i2s_config_t primary_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = true,
    .tx_desc_auto_clear = true
  };
  
  if (i2s_driver_install(I2S_NUM_0, &primary_config, 0, NULL) != ESP_OK) {
    Serial.println("Primary I2S driver install failed!");
    while(1);
  }
  
  i2s_pin_config_t primary_pins = {
    .bck_io_num = PRIMARY_SCK,
    .ws_io_num = PRIMARY_WS,
    .data_out_num = AMP_DOUT,
    .data_in_num = PRIMARY_SD
  };
  
  if (i2s_set_pin(I2S_NUM_0, &primary_pins) != ESP_OK) {
    Serial.println("Primary I2S pin setup failed!");
    while(1);
  }
  
  // Secondary I2S configuration (Mic2 only)
  i2s_config_t secondary_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = true
  };
  
  if (i2s_driver_install(I2S_NUM_1, &secondary_config, 0, NULL) != ESP_OK) {
    Serial.println("Secondary I2S driver install failed!");
    while(1);
  }
  
  i2s_pin_config_t secondary_pins = {
    .bck_io_num = SECONDARY_SCK,
    .ws_io_num = SECONDARY_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = SECONDARY_SD,
  };
  
  if (i2s_set_pin(I2S_NUM_1, &secondary_pins) != ESP_OK) {
    Serial.println("Secondary I2S pin setup failed!");
    while(1);
  }
  
  // Initialize I2S buffers and start
  i2s_zero_dma_buffer(I2S_NUM_0);
  i2s_zero_dma_buffer(I2S_NUM_1);
  i2s_start(I2S_NUM_0);
  i2s_start(I2S_NUM_1);
  
  Serial.println("System ready!");
  Serial.println("Press and hold button to record noise-cancelled audio, release to play");
  Serial.println("RMS Error monitoring started...");
}

void loop() {
  bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW);
  
  // Handle button press/release for recording control
  if (buttonPressed && !wasButtonPressed) {
    startRecording();
  }
  
  if (!buttonPressed && wasButtonPressed && isRecording) {
    stopRecording();
    playRecording();
  }
  
  wasButtonPressed = buttonPressed;
  
  // Read audio from both microphones
  int16_t mic1_samples[SAMPLE_BUFFER_SIZE];  // Primary (desired + noise)
  int16_t mic2_samples[SAMPLE_BUFFER_SIZE];  // Reference (noise)
  size_t bytes_read1, bytes_read2;
  
  // Read from both I2S buses
  if (i2s_read(I2S_NUM_0, mic1_samples, BYTES_TO_READ, &bytes_read1, pdMS_TO_TICKS(10)) != ESP_OK ||
      i2s_read(I2S_NUM_1, mic2_samples, BYTES_TO_READ, &bytes_read2, pdMS_TO_TICKS(10)) != ESP_OK ||
      bytes_read1 == 0 || bytes_read2 == 0) {
    return;
  }
  
  // Process samples with LMS adaptive filter
  int sample_count = min(bytes_read1, bytes_read2) / sizeof(int16_t);
  int16_t filtered_samples[SAMPLE_BUFFER_SIZE];
  
  for(int i = 0; i < sample_count; i++) {
    // Normalize inputs to float range [-1, 1]
    float primary_input = (float)mic1_samples[i] / 32768.0;      // d(n) - desired signal + noise
    float reference_input = (float)mic2_samples[i] / 32768.0;    // x(n) - reference noise
    
    // Apply LMS adaptive filtering
    float filtered_output = applyLMSFilter(primary_input, reference_input);
    
    // Apply gain and convert back to int16
    filtered_output *= OUTPUT_GAIN;
    
    // Apply noise gate
    if (abs(filtered_output * 32768) < NOISE_GATE) {
      filtered_output = 0;
    }
    
    // Convert back to int16 and clamp
    int32_t output_sample = (int32_t)(filtered_output * 32768.0);
    filtered_samples[i] = constrain(output_sample, -32768, 32767);
  }
  
  // If recording, save filtered audio to SD card
  if (isRecording && recordingFile) {
    size_t bytes_to_write = sample_count * sizeof(int16_t);
    recordingFile.write((uint8_t*)filtered_samples, bytes_to_write);
  }
  
  // Always output filtered audio for live monitoring
  size_t bytes_written;
  i2s_write(I2S_NUM_0, filtered_samples, sample_count * sizeof(int16_t), &bytes_written, pdMS_TO_TICKS(10));
  
  // Print RMS error periodically
  unsigned long currentTime = millis();
  if (currentTime - lastRMSPrint >= 500) {  // Print every 500ms
    printRMSError();
    lastRMSPrint = currentTime;
  }
}

float applyLMSFilter(float desired, float reference) {
  // Shift input buffer (delay line for reference signal)
  for(int i = FILTER_LENGTH - 1; i > 0; i--) {
    inputBuffer[i] = inputBuffer[i-1];
  }
  inputBuffer[0] = reference;
  
  // Calculate filter output y(n) = w^T(n) * x(n)
  float filterOutput = 0.0;
  for(int i = 0; i < FILTER_LENGTH; i++) {
    filterOutput += filterCoeffs[i] * inputBuffer[i];
  }
  
  // Calculate error e(n) = d(n) - y(n)
  float error = desired - filterOutput;
  
  // Store error for RMS calculation
  errorBuffer[errorIndex] = error;
  errorIndex = (errorIndex + 1) % RMS_WINDOW_SIZE;
  
  // Update filter coefficients using LMS: w(n+1) = w(n) + μ * e(n) * x(n)
  if (!hasConverged) {
    for(int i = 0; i < FILTER_LENGTH; i++) {
      filterCoeffs[i] += LMS_MU * error * inputBuffer[i];
    }
  }
  
  // Return the error as the cleaned signal (noise cancelled)
  return error;
}

float calculateRMSError() {
  float sumSquares = 0.0;
  for(int i = 0; i < RMS_WINDOW_SIZE; i++) {
    sumSquares += errorBuffer[i] * errorBuffer[i];
  }
  return sqrt(sumSquares / RMS_WINDOW_SIZE);
}

void printRMSError() {
  float currentRMS = calculateRMSError();
  
  // Check for convergence
  if (!hasConverged) {
    float rmsChange = abs(currentRMS - lastRMSError);
    if (rmsChange < CONVERGENCE_THRESHOLD && lastRMSError > 0) {
      hasConverged = true;
      Serial.println("🎯 FILTER CONVERGED! Noise cancellation optimized.");
      Serial.println("=====================================");
    }
  }
  
  // Print current status
  if (!hasConverged) {
    Serial.printf("🔄 Adapting... RMS Error: %.6f | Change: %.6f\n", 
                  currentRMS, abs(currentRMS - lastRMSError));
  } else {
    Serial.printf("✅ Converged   RMS Error: %.6f | Noise Suppression Active\n", currentRMS);
  }
  
  lastRMSError = currentRMS;
}

void startRecording() {
  Serial.println("🎤 Recording started (noise-cancelled audio)...");
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
      Serial.printf("📁 Noise-cancelled audio file size: %d bytes\n", tempFile.size());
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
  
  Serial.println("🔊 Playing noise-cancelled recording...");
  int16_t playBuffer[SAMPLE_BUFFER_SIZE];
  
  while (playFile.available()) {
    int bytesRead = playFile.read((uint8_t*)playBuffer, BYTES_TO_READ);
    if (bytesRead > 0) {
      size_t bytesWritten;
      i2s_write(I2S_NUM_0, playBuffer, bytesRead, &bytesWritten, pdMS_TO_TICKS(100));
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
