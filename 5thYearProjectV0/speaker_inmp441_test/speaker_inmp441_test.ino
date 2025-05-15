#include <Arduino.h>
#include <driver/i2s.h>

// Settings
#define SAMPLE_BUFFER_SIZE 512
#define SAMPLE_RATE 8000

// I2S Pins for Microphone (INMP441)
#define I2S_MIC_SERIAL_CLOCK GPIO_NUM_14
#define I2S_MIC_LEFT_RIGHT_CLOCK GPIO_NUM_15
#define I2S_MIC_SERIAL_DATA GPIO_NUM_32

// I2S Pins for Speaker (MAX98357A)
#define I2S_SPK_SERIAL_CLOCK GPIO_NUM_26
#define I2S_SPK_LEFT_RIGHT_CLOCK GPIO_NUM_25
#define I2S_SPK_SERIAL_DATA GPIO_NUM_22

// I2S configuration for microphone (input)
i2s_config_t i2s_mic_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0};

// I2S configuration for speaker (output)
i2s_config_t i2s_spk_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // MAX98357A works best with 16-bit
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0};

// Pin configuration for microphone
i2s_pin_config_t i2s_mic_pins = {
    .bck_io_num = I2S_MIC_SERIAL_CLOCK,
    .ws_io_num = I2S_MIC_LEFT_RIGHT_CLOCK,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SERIAL_DATA};

// Pin configuration for speaker
i2s_pin_config_t i2s_spk_pins = {
    .bck_io_num = I2S_SPK_SERIAL_CLOCK,
    .ws_io_num = I2S_SPK_LEFT_RIGHT_CLOCK,
    .data_out_num = I2S_SPK_SERIAL_DATA,
    .data_in_num = I2S_PIN_NO_CHANGE};

// Buffers for audio data
int32_t raw_samples[SAMPLE_BUFFER_SIZE];
int16_t output_samples[SAMPLE_BUFFER_SIZE]; // MAX98357A uses 16-bit samples

void setup() {
  Serial.begin(115200);
  
  // Initialize I2S for microphone (input)
  i2s_driver_install(I2S_NUM_0, &i2s_mic_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &i2s_mic_pins);
  
  // Initialize I2S for speaker (output)
  i2s_driver_install(I2S_NUM_1, &i2s_spk_config, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &i2s_spk_pins);
}

void loop() {
  size_t bytes_read = 0;
  
  // Read audio data from microphone
  i2s_read(I2S_NUM_0, raw_samples, sizeof(int32_t) * SAMPLE_BUFFER_SIZE, &bytes_read, portMAX_DELAY);
  int samples_read = bytes_read / sizeof(int32_t);
  
  // Process samples and prepare for output
  for (int i = 0; i < samples_read; i++) {
    // Convert 32-bit sample to 16-bit by dropping the lower bits
    output_samples[i] = (int16_t)(raw_samples[i] >> 11); // Adjust shift as needed
    
    // Optional: Send to serial plotter (comment out if not needed)
    Serial.println(raw_samples[i]);
  }
  
  // Write audio data to speaker
  size_t bytes_written = 0;
  i2s_write(I2S_NUM_1, output_samples, samples_read * sizeof(int16_t), &bytes_written, portMAX_DELAY);
}
