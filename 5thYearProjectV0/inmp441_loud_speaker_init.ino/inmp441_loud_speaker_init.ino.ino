#include <driver/i2s.h>

#define I2S_SCK 14
#define I2S_WS 15
#define I2S_SD 32  // Mic data
#define I2S_DOUT 25 // Amp data

void setup() {
  Serial.begin(115200);
  
  // I2S Configuration (Optimized for Volume)
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate = 44100,  // Higher sample rate reduces distortion
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,    // Lower latency
    .dma_buf_len = 512,    // Smaller buffers = faster response
    .use_apll = true       // High-precision clock for cleaner output
  };
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);

  // Pin config (same as before)
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_SD
  };
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

void loop() {
  int16_t raw_samples[512];  // Buffer for mic data
  size_t bytes_read;
  
  // 1. Read from microphone
  i2s_read(I2S_NUM_0, raw_samples, sizeof(raw_samples), &bytes_read, portMAX_DELAY);
  Serial.print("I am bytes_read value: ");
  Serial.println(bytes_read);
  Serial.print("Sizeof(int16_t)is: ");
  Serial.println(sizeof(int16_t));
  // 2. Apply volume boost (critical for loudness)
  int samples_count = bytes_read / sizeof(int16_t);
  Serial.print("I am samples_count: ");
  Serial.println(samples_count);
  for(int i = 0; i < samples_count; i++) {
    // Multiply sample by gain (3x = ~9.5dB boost)
    int32_t boosted = raw_samples[i] * 3;
    
    // Prevent clipping (hard limit to 16-bit range)
    raw_samples[i] = constrain(boosted, -32768, 32767);
  }

  // 3. Send to amplifier
  size_t bytes_written;
  i2s_write(I2S_NUM_0, raw_samples, bytes_read, &bytes_written, portMAX_DELAY);
}
