#include <driver/i2s.h>

// Primary I2S Bus (Mic1 + Amp)
#define PRIMARY_SCK 14  // Shared BCLK
#define PRIMARY_WS 15   // Shared LRC
#define PRIMARY_SD 32   // Mic1 data
#define AMP_DOUT 25    // Amp data

// Secondary I2S Bus (Mic2)
#define SECONDARY_SCK 14  // MUST share clock with primary!
#define SECONDARY_WS 15   // MUST share word select with primary!
#define SECONDARY_SD 33   // Mic2 data

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
    .dma_buf_count = 8,  // Increased for stability
    .dma_buf_len = 512,
    .use_apll = true     // Only one peripheral should use APLL
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
    .sample_rate = 44100,  // MUST match primary exactly
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 512,
    .use_apll = false      // Must be false when sharing clock
  };
  i2s_driver_install(I2S_NUM_1, &secondary_config, 0, NULL);

  // Secondary pin config - MUST share clock and WS pins!
  i2s_pin_config_t secondary_pins = {
    .bck_io_num = SECONDARY_SCK,  // Same as primary SCK
    .ws_io_num = SECONDARY_WS,    // Same as primary WS
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = SECONDARY_SD
  };
  i2s_set_pin(I2S_NUM_1, &secondary_pins);

  Serial.println("I2S setup complete");
}

void loop() {
  int16_t mic1_samples[256];
  int16_t mic2_samples[256];
  size_t bytes_read1, bytes_read2;

  // Read from both mics with timeout
  esp_err_t err1 = i2s_read(I2S_NUM_0, mic1_samples, sizeof(mic1_samples), &bytes_read1, 0);
  esp_err_t err2 = i2s_read(I2S_NUM_1, mic2_samples, sizeof(mic2_samples), &bytes_read2, 0);

  // Debug output
  if(err1 != ESP_OK || err2 != ESP_OK) {
    Serial.printf("I2S read errors: Mic1=%d, Mic2=%d\n", err1, err2);
    delay(100);
    return;
  }

  // Verify we got data from both mics
  if(bytes_read1 > 0 && bytes_read2 > 0) {
    // Mix samples with adjustable gains
    int sample_count = min(bytes_read1, bytes_read2) / sizeof(int16_t);
    for(int i = 0; i < sample_count; i++) {
      int32_t mixed = (mic1_samples[i] * 1.0) + (mic2_samples[i] * 1.0); // Adjust gains here
      mixed = mixed / 2;  // Normalize
      mic1_samples[i] = constrain(mixed, -32768, 32767);
    }
    
    // Output mixed audio
    size_t bytes_written;
    i2s_write(I2S_NUM_0, mic1_samples, bytes_read1, &bytes_written, portMAX_DELAY);
  }
  
  // Debug output
  static uint32_t last_print = 0;
  if(millis() - last_print > 1000) {
    Serial.printf("Mic1: %d bytes, Mic2: %d bytes\n", bytes_read1, bytes_read2);
    last_print = millis();
  }
}
