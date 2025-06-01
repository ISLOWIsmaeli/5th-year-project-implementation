#include <driver/i2s.h>

// Simple buffer for audio data
#define BUFFER_SIZE 256
int16_t mic1_data[BUFFER_SIZE];
int16_t mic2_data[BUFFER_SIZE];
int16_t speaker_data[BUFFER_SIZE];

// Pin definitions
#define MIC1_CLK 14    // Clock pin for microphone 1
#define MIC1_WS  15    // Word select pin for microphone 1  
#define MIC1_DATA 32   // Data pin for microphone 1

#define MIC2_CLK 26    // Clock pin for microphone 2
#define MIC2_WS  25    // Word select pin for microphone 2
#define MIC2_DATA 35   // Data pin for microphone 2

#define SPEAKER_DATA 22 // Speaker output pin

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Dual Microphone System...");
  
  // Setup first microphone + speaker (uses I2S port 0)
  setupMic1AndSpeaker();
  
  // Setup second microphone (uses I2S port 1)  
  setupMic2();
  
  Serial.println("System ready! Speak into the microphones.");
}

void setupMic1AndSpeaker() {
  // Configuration for microphone 1 and speaker
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,  // Different interrupt level
    .dma_buf_count = 2,
    .dma_buf_len = BUFFER_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };
  
  // Install the driver
  esp_err_t result = i2s_driver_install(I2S_NUM_0, &config, 0, NULL);
  if (result != ESP_OK) {
    Serial.println("ERROR: Failed to install I2S0 driver!");
    Serial.print("Error code: ");
    Serial.println(result);
    return;
  }
  
  // Set the pins
  i2s_pin_config_t pins = {
    .bck_io_num = MIC1_CLK,
    .ws_io_num = MIC1_WS,
    .data_out_num = SPEAKER_DATA,  // Speaker output
    .data_in_num = MIC1_DATA       // Microphone input
  };
  
  result = i2s_set_pin(I2S_NUM_0, &pins);
  if (result != ESP_OK) {
    Serial.println("ERROR: Failed to set I2S0 pins!");
    return;
  }
  
  // Start the I2S driver
  i2s_start(I2S_NUM_0);
  
  Serial.println("Microphone 1 and Speaker ready and started");
}

void setupMic2() {
  // Configuration for microphone 2 only
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,  // Different interrupt level
    .dma_buf_count = 2,
    .dma_buf_len = BUFFER_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = false,  // Not needed for RX only
    .fixed_mclk = 0
  };
  
  // Install the driver
  esp_err_t result = i2s_driver_install(I2S_NUM_1, &config, 0, NULL);
  if (result != ESP_OK) {
    Serial.println("ERROR: Failed to install I2S1 driver!");
    Serial.print("Error code: ");
    Serial.println(result);
    return;
  }
  
  // Set the pins
  i2s_pin_config_t pins = {
    .bck_io_num = MIC2_CLK,
    .ws_io_num = MIC2_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = MIC2_DATA
  };
  
  result = i2s_set_pin(I2S_NUM_1, &pins);
  if (result != ESP_OK) {
    Serial.println("ERROR: Failed to set I2S1 pins!");
    return;
  }
  
  // Start the I2S driver
  i2s_start(I2S_NUM_1);
  
  Serial.println("Microphone 2 ready and started");
}

void loop() {
  size_t mic1_bytes, mic2_bytes, speaker_bytes;
  
  // Read audio from both microphones with short timeout
  i2s_read(I2S_NUM_0, mic1_data, sizeof(mic1_data), &mic1_bytes, 10);
  i2s_read(I2S_NUM_1, mic2_data, sizeof(mic2_data), &mic2_bytes, 10);
  
  // Debug: Print status every 500 loops to check if mic2 is working
  static int debug_count = 0;
  debug_count++;
  if (debug_count % 500 == 0) {
    Serial.print("Mic1 bytes: ");
    Serial.print(mic1_bytes);
    Serial.print(" | Mic2 bytes: ");
    Serial.println(mic2_bytes);
  }
  
  // Convert bytes to number of audio samples
  int mic1_samples = mic1_bytes / 2;  // 2 bytes per sample (16-bit)
  int mic2_samples = mic2_bytes / 2;
  
  // Process audio if we have data from either microphone
  if (mic1_samples > 0 || mic2_samples > 0) {
    
    // Find how many samples to process (use the larger amount)
    int total_samples = max(mic1_samples, mic2_samples);
    
    // Mix the audio from both microphones
    for (int i = 0; i < total_samples; i++) {
      int mixed_audio = 0;
      
      // Add microphone 1 audio if we have it
      if (i < mic1_samples) {
        mixed_audio += mic1_data[i];
      }
      
      // Add microphone 2 audio if we have it
      if (i < mic2_samples) {
        mixed_audio += mic2_data[i];
      }
      
      // Make it louder
      mixed_audio = mixed_audio * 2;
      
      // Prevent audio clipping using constrain
      speaker_data[i] = constrain(mixed_audio, -32768, 32767);
    }
    
    // Send mixed audio to speaker immediately
    i2s_write(I2S_NUM_0, speaker_data, total_samples * 2, &speaker_bytes, 10);
  }
}
