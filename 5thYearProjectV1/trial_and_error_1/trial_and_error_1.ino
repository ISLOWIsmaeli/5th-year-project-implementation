#include <driver/i2s.h>

// Primary I2S Bus (Mic1 + Amp)
#define PRIMARY_SCK 14
#define PRIMARY_WS 15
#define PRIMARY_SD 32
#define AMP_DOUT 22

// Secondary I2S Bus (Mic2)
#define SECONDARY_SCK 26
#define SECONDARY_WS 25
#define SECONDARY_SD 35

// Buffer size - reduced for lower latency
#define SAMPLE_BUFFER_SIZE 128
#define BYTES_TO_READ (SAMPLE_BUFFER_SIZE * sizeof(int16_t))

void setup() {
  Serial.begin(115200);
  
  // Primary I2S Config (TX to amp + RX from mic1)
  i2s_config_t primary_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,        // Increased buffer count
    .dma_buf_len = 256,        // Reduced buffer length for lower latency
    .use_apll = true,
    .tx_desc_auto_clear = true // Clear TX descriptor automatically
  };
  
  i2s_driver_install(I2S_NUM_0, &primary_config, 0, NULL);
  
  // Primary pin config
  i2s_pin_config_t primary_pins = {
    .bck_io_num = PRIMARY_SCK,
    .ws_io_num = PRIMARY_WS,
    .data_out_num = AMP_DOUT,
    .data_in_num = PRIMARY_SD
  };
  i2s_set_pin(I2S_NUM_0, &primary_pins);
  
  // Secondary I2S Config (RX from mic2 only)
  i2s_config_t secondary_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,        // Increased buffer count
    .dma_buf_len = 256,        // Consistent with primary
    .use_apll = true           // Changed to true for better sync
  };
  
  i2s_driver_install(I2S_NUM_1, &secondary_config, 0, NULL);
  
  // Secondary pin config
  i2s_pin_config_t secondary_pins = {
    .bck_io_num = SECONDARY_SCK,
    .ws_io_num = SECONDARY_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = SECONDARY_SD,
  };
  i2s_set_pin(I2S_NUM_1, &secondary_pins);
  
  // Clear any existing data in the I2S buffers
  i2s_zero_dma_buffer(I2S_NUM_0);
  i2s_zero_dma_buffer(I2S_NUM_1);
  
  // Start both I2S peripherals
  i2s_start(I2S_NUM_0);
  i2s_start(I2S_NUM_1);
  
  Serial.println("I2S initialized successfully");
}

void loop() {
  int16_t mic1_samples[SAMPLE_BUFFER_SIZE];
  int16_t mic2_samples[SAMPLE_BUFFER_SIZE];
  size_t bytes_read1, bytes_read2;
  
  // Clear buffers before reading
  memset(mic1_samples, 0, sizeof(mic1_samples));
  memset(mic2_samples, 0, sizeof(mic2_samples));
  
  // Read from both mics with timeout
  esp_err_t err1 = i2s_read(I2S_NUM_0, mic1_samples, BYTES_TO_READ, &bytes_read1, pdMS_TO_TICKS(10));
  esp_err_t err2 = i2s_read(I2S_NUM_1, mic2_samples, BYTES_TO_READ, &bytes_read2, pdMS_TO_TICKS(10));
  
  if (err1 != ESP_OK || err2 != ESP_OK) {
    Serial.printf("Read error: mic1=%d, mic2=%d\n", err1, err2);
    return;
  }
  
  // Ensure we have valid data
  if (bytes_read1 == 0 || bytes_read2 == 0) {
    Serial.println("No data read from microphones");
    return;
  }
  
  // Calculate actual sample count
  int sample_count1 = bytes_read1 / sizeof(int16_t);
  int sample_count2 = bytes_read2 / sizeof(int16_t);
  int sample_count = min(sample_count1, sample_count2);
  
  if (sample_count <= 0) {
    return;
  }
  
  // Mix samples with proper scaling and overflow protection
  for(int i = 0; i < sample_count; i++) {
    // Convert to 32-bit for calculation
    int32_t sample1 = mic1_samples[i];
    int32_t sample2 = mic2_samples[i];
    
    // Simple average mixing
    int32_t mixed = (sample1 + sample2) / 2;
    
    // Clamp to 16-bit range
    if (mixed > 32767) mixed = 32767;
    if (mixed < -32768) mixed = -32768;
    
    mic1_samples[i] = (int16_t)mixed;
  }
  
  // Output mixed audio - use actual bytes read for consistency
  size_t bytes_to_write = sample_count * sizeof(int16_t);
  size_t bytes_written;
  
  esp_err_t write_err = i2s_write(I2S_NUM_0, mic1_samples, bytes_to_write, &bytes_written, pdMS_TO_TICKS(10));
  
  if (write_err != ESP_OK) {
    Serial.printf("Write error: %d\n", write_err);
  }
  
  // Optional: Print debug info periodically
  static int debug_counter = 0;
  if (++debug_counter % 1000 == 0) {
    Serial.printf("Samples: mic1=%d, mic2=%d, mixed=%d, written=%d\n", 
                  sample_count1, sample_count2, sample_count, bytes_written/sizeof(int16_t));
  }
}
