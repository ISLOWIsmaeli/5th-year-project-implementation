#include <driver/i2s.h>

// Pins configuration
#define I2S_MIC_SD 32
#define I2S_AMP_DIN 25
#define I2S_SCK 14
#define I2S_WS 15

// I2S settings
#define SAMPLE_RATE 44100
#define BUFFER_SIZE 1024
#define BITS_PER_SAMPLE 16

void setup() {
  Serial.begin(115200);
  
  // Configure I2S for both input (mic) and output (amp)
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = (i2s_bits_per_sample_t)BITS_PER_SAMPLE,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = BUFFER_SIZE,
    .use_apll = false
  };

  // Install and start I2S driver
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);

  // Set up pins for both input and output
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_AMP_DIN,
    .data_in_num = I2S_MIC_SD
  };
  i2s_set_pin(I2S_NUM_0, &pin_config);

  Serial.println("Audio loopback system ready");
}

void loop() {
  int16_t audio_samples[BUFFER_SIZE];
  size_t bytes_read;
  
  // Read from microphone
  i2s_read(I2S_NUM_0, &audio_samples, BUFFER_SIZE * sizeof(int16_t), &bytes_read, portMAX_DELAY);
  
  // Optional: Add audio processing here (e.g., volume adjustment)
  // processAudio(audio_samples, bytes_read/sizeof(int16_t));
  
  // Write to amplifier
  size_t bytes_written;
  i2s_write(I2S_NUM_0, &audio_samples, bytes_read, &bytes_written, portMAX_DELAY);
  
  // Simple delay to prevent watchdog triggers
  delay(1);
}

// Optional audio processing function
void processAudio(int16_t* samples, size_t count) {
  // Example: Reduce volume by 50%
  for(int i=0; i<count; i++) {
    samples[i] = samples[i] / 2;
  }
}
