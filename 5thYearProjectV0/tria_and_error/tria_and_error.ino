#include <driver/i2s.h>
#include <math.h>

// Pins & Config
#define PRIMARY_SCK 14
#define PRIMARY_WS 15
#define PRIMARY_SD 32
#define AMP_DOUT 25
#define SECONDARY_SCK 18
#define SECONDARY_WS 2
#define SECONDARY_SD 33

// ANC Parameters
#define FILTER_TAPS 32    // Number of filter coefficients (tradeoff between performance and computation)
#define MU 0.01f         // Learning rate (critical for stability)

// LMS Filter Variables
float w[FILTER_TAPS] = {0};  // Adaptive filter coefficients
float x_history[FILTER_TAPS] = {0};  // History of reference samples

// Serial Plotter Settings
const int PLOT_SAMPLE_INTERVAL = 10;
unsigned long lastPlotTime = 0;

void setup() {
  Serial.begin(115200);
  
  // I2S Setup (same as before)
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
  
  i2s_pin_config_t primary_pins = {
    .bck_io_num = PRIMARY_SCK,
    .ws_io_num = PRIMARY_WS,
    .data_out_num = AMP_DOUT,
    .data_in_num = PRIMARY_SD
  };
  i2s_set_pin(I2S_NUM_0, &primary_pins);

  // Secondary I2S (Mic2)
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
  
  i2s_pin_config_t secondary_pins = {
    .bck_io_num = SECONDARY_SCK,
    .ws_io_num = SECONDARY_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = SECONDARY_SD,
  };
  i2s_set_pin(I2S_NUM_1, &secondary_pins);
}

// LMS Adaptive Filter Implementation
float lms_filter(float primary, float reference) {
  static int index = 0;
  
  // Update reference sample history
  x_history[index] = reference;
  
  // Calculate filter output (noise estimate)
  float noise_estimate = 0;
  for (int i = 0; i < FILTER_TAPS; i++) {
    int j = (index + i) % FILTER_TAPS;
    noise_estimate += w[i] * x_history[j];
  }
  
  // Calculate error (desired signal)
  float error = primary - noise_estimate;
  
  // Update filter coefficients
  for (int i = 0; i < FILTER_TAPS; i++) {
    int j = (index + i) % FILTER_TAPS;
    w[i] += MU * error * x_history[j];
  }
  
  index = (index + 1) % FILTER_TAPS;
  return error; // Return cleaned signal
}

void loop() {
  int16_t mic1_samples[256];
  int16_t mic2_samples[256];
  size_t bytes_read1, bytes_read2;

  // 1. Read audio data
  i2s_read(I2S_NUM_0, mic1_samples, sizeof(mic1_samples), &bytes_read1, portMAX_DELAY);
  i2s_read(I2S_NUM_1, mic2_samples, sizeof(mic2_samples), &bytes_read2, portMAX_DELAY);

  int sample_count = min(bytes_read1, bytes_read2) / sizeof(int16_t);
  
  // 2. Process samples with ANC
  for(int i = 0; i < sample_count; i++) {
    // Convert to float for processing (-1.0 to 1.0 range)
    float primary = mic1_samples[i] / 32768.0f;
    float reference = mic2_samples[i] / 32768.0f;
    
    // Apply adaptive noise cancellation
    float cleaned = lms_filter(primary, reference);
    
    // Apply gain and convert back to int16
//    mic1_samples[i] = constrain((int16_t)(cleaned * 32767.0f * 2.0), -32768, 32767);
    // Soft clipping using tanh (alternative to constrain)
    mic1_samples[i] = (int16_t)(32767.0f * tanh(cleaned));
    
    // Optional: Send to Serial Plotter
    if (millis() - lastPlotTime > 10 && i == 0) {
      lastPlotTime = millis();
      Serial.print("Raw:");
      Serial.print(mic1_samples[i]);
      Serial.print(",Cleaned:");
      Serial.println((int16_t)(cleaned * 32767.0f));
    }
  }

  // 3. Output processed audio
  size_t bytes_written;
  i2s_write(I2S_NUM_0, mic1_samples, bytes_read1, &bytes_written, portMAX_DELAY);
}
