#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

// Settings
#define SAMPLE_BUFFER_SIZE 512
#define SAMPLE_RATE 16000  // Increased sample rate for better quality
#define TARGET_GAIN 8.0f   // Adjust this for output volume

// I2S Pins
#define I2S_MIC_SERIAL_CLOCK GPIO_NUM_14
#define I2S_MIC_LEFT_RIGHT_CLOCK GPIO_NUM_15
#define I2S_MIC_SERIAL_DATA GPIO_NUM_32
#define I2S_SPK_SERIAL_CLOCK GPIO_NUM_26
#define I2S_SPK_LEFT_RIGHT_CLOCK GPIO_NUM_25
#define I2S_SPK_SERIAL_DATA GPIO_NUM_22

// Audio processing parameters
#define DC_HISTORY_SIZE 32
#define NOISE_GATE_THRESHOLD 500  // Adjust based on your noise floor
#define LOWPASS_FREQ 4000         // 4kHz low-pass cutoff

// I2S Configurations
i2s_config_t i2s_mic_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,
    .use_apll = true,  // Better clock quality
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0};

i2s_config_t i2s_spk_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,
    .use_apll = true,  // Better clock quality
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0};

// Pin configurations
i2s_pin_config_t i2s_mic_pins = {
    .bck_io_num = I2S_MIC_SERIAL_CLOCK,
    .ws_io_num = I2S_MIC_LEFT_RIGHT_CLOCK,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SERIAL_DATA};

i2s_pin_config_t i2s_spk_pins = {
    .bck_io_num = I2S_SPK_SERIAL_CLOCK,
    .ws_io_num = I2S_SPK_LEFT_RIGHT_CLOCK,
    .data_out_num = I2S_SPK_SERIAL_DATA,
    .data_in_num = I2S_PIN_NO_CHANGE};

// Audio processing variables
float dc_offset = 0;
float dc_history[DC_HISTORY_SIZE];
int dc_history_index = 0;
float lowpass_prev = 0;
const float lowpass_alpha = 2 * M_PI * LOWPASS_FREQ / SAMPLE_RATE;

// Buffers
int32_t raw_samples[SAMPLE_BUFFER_SIZE];
int16_t output_samples[SAMPLE_BUFFER_SIZE];

void setup() {
  Serial.begin(115200);
  
  // Initialize I2S
  i2s_driver_install(I2S_NUM_0, &i2s_mic_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &i2s_mic_pins);
  i2s_driver_install(I2S_NUM_1, &i2s_spk_config, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &i2s_spk_pins);

  // Initialize DC offset history
  for(int i=0; i<DC_HISTORY_SIZE; i++) {
    dc_history[i] = 0;
  }
}

float process_sample(int32_t sample) {
  // 1. Convert to float (-1.0 to 1.0 range)
  float fsample = sample / 2147483648.0f;  // 2^31
  
  // 2. DC offset removal (adaptive)
  dc_history[dc_history_index] = fsample;
  dc_history_index = (dc_history_index + 1) % DC_HISTORY_SIZE;
  
  float dc_sum = 0;
  for(int i=0; i<DC_HISTORY_SIZE; i++) {
    dc_sum += dc_history[i];
  }
  dc_offset = dc_sum / DC_HISTORY_SIZE;
  fsample -= dc_offset;
  
  // 3. Noise gate (soft knee)
  float abs_sample = fabs(fsample);
  if(abs_sample < NOISE_GATE_THRESHOLD/2147483648.0f) {
    fsample *= abs_sample * (2147483648.0f/NOISE_GATE_THRESHOLD);
  }
  
  // 4. Simple low-pass filter
  lowpass_prev = lowpass_prev + lowpass_alpha * (fsample - lowpass_prev);
  fsample = lowpass_prev;
  
  // 5. Apply gain
  fsample *= TARGET_GAIN;
  
  // 6. Clipping protection
  if(fsample > 0.95f) fsample = 0.95f;
  if(fsample < -0.95f) fsample = -0.95f;
  
  return fsample;
}

void loop() {
  size_t bytes_read = 0;
  
  // Read from microphone
  i2s_read(I2S_NUM_0, raw_samples, sizeof(int32_t) * SAMPLE_BUFFER_SIZE, &bytes_read, portMAX_DELAY);
  int samples_read = bytes_read / sizeof(int32_t);
  
  // Process and output
  for(int i=0; i<samples_read; i++) {
    float processed = process_sample(raw_samples[i]);
    output_samples[i] = (int16_t)(processed * 32767.0f);
  }
  
  // Write to speaker
  size_t bytes_written = 0;
  i2s_write(I2S_NUM_1, output_samples, samples_read * sizeof(int16_t), &bytes_written, portMAX_DELAY);
  
  // Optional: Print processing stats occasionally
  static unsigned long last_print = 0;
  if(millis() - last_print > 1000) {
    Serial.printf("DC Offset: %.6f, Samples: %d\n", dc_offset, samples_read);
    last_print = millis();
  }
}
