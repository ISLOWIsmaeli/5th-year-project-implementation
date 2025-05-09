#include <driver/i2s.h>

// Primary I2S Bus (Mic1 + Amp)
#define PRIMARY_SCK 14
#define PRIMARY_WS 15
#define PRIMARY_SD 32
#define AMP_DOUT 25

// Secondary I2S Bus (Mic2)
#define SECONDARY_SCK 18
#define SECONDARY_WS 2
#define SECONDARY_SD 33

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
    .dma_buf_count = 4,
    .dma_buf_len = 512,
    .use_apll = true
  };
  i2s_driver_install(I2S_NUM_0, &primary_config, 0, NULL);

  // Primary pin config - CORRECT ORDER
  i2s_pin_config_t primary_pins = {
    .bck_io_num = PRIMARY_SCK,
    .ws_io_num = PRIMARY_WS,
    .data_out_num = AMP_DOUT,     // Output comes after input
    .data_in_num = PRIMARY_SD    // Input comes first in ESP-IDF v4.4+
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
    .dma_buf_count = 4,
    .dma_buf_len = 512,
    .use_apll = false
  };
  i2s_driver_install(I2S_NUM_1, &secondary_config, 0, NULL);

  // Secondary pin config - CORRECT ORDER
  i2s_pin_config_t secondary_pins = {
    .bck_io_num = SECONDARY_SCK,
    .ws_io_num = SECONDARY_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = SECONDARY_SD,
  };
  i2s_set_pin(I2S_NUM_1, &secondary_pins);
}

void loop() {
  int16_t mic1_samples[256];
  int16_t mic2_samples[256];
  size_t bytes_read1, bytes_read2;

  // Read from both mics
  i2s_read(I2S_NUM_0, mic1_samples, sizeof(mic1_samples), &bytes_read1, portMAX_DELAY);
  i2s_read(I2S_NUM_1, mic2_samples, sizeof(mic2_samples), &bytes_read2, portMAX_DELAY);

  // Mix samples (simple average)
  int sample_count = min(bytes_read1, bytes_read2) / sizeof(int16_t);
  for(int i = 0; i < sample_count; i++) {
    int32_t mixed = (mic1_samples[i] + mic2_samples[i]) / 2;
    mic1_samples[i] = constrain(mixed, -32768, 32767);
  }

  // Output mixed audio
  size_t bytes_written;
  i2s_write(I2S_NUM_0, mic1_samples, bytes_read1, &bytes_written, portMAX_DELAY);
}

// only the first microphone is outputing whereas the 2nd microphone is not displaying
