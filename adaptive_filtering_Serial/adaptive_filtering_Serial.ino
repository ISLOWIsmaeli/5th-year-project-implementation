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
#define FILTER_TAPS 32
#define MU 0.00001f
#define RMSE_WINDOW 100
#define SERIAL_OUTPUT_INTERVAL_MS 50  // Update plot every 50ms

// LMS Filter Variables
float w[FILTER_TAPS] = {0};
float x_history[FILTER_TAPS] = {0};

// Error tracking
float error_buffer[RMSE_WINDOW] = {0};
int error_index = 0;
unsigned long lastSerialOutput = 0;
unsigned long iterationCount = 0;

void setup() {
  Serial.begin(115200);
  
  // I2S Setup
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

  Serial.println("Time(ms),Iteration,RMSE"); // CSV header
}

float calculate_rmse() {
  float sum_sq = 0;
  for (int i = 0; i < RMSE_WINDOW; i++) {
    sum_sq += error_buffer[i] * error_buffer[i];
  }
  return sqrt(sum_sq / RMSE_WINDOW);
}

float lms_filter(float primary, float reference) {
  static int index = 0;
  
  x_history[index] = reference;
  
  float noise_estimate = 0;
  for (int i = 0; i < FILTER_TAPS; i++) {
    int j = (index + i) % FILTER_TAPS;
    noise_estimate += w[i] * x_history[j];
  }
  
  float error = primary - noise_estimate;
  error_buffer[error_index] = error;
  error_index = (error_index + 1) % RMSE_WINDOW;
  
  for (int i = 0; i < FILTER_TAPS; i++) {
    int j = (index + i) % FILTER_TAPS;
    w[i] += MU * error * x_history[j];
  }
  
  index = (index + 1) % FILTER_TAPS;
  return error;
}

void loop() {
  int16_t mic1_samples[256];
  int16_t mic2_samples[256];
  size_t bytes_read1, bytes_read2;

  i2s_read(I2S_NUM_0, mic1_samples, sizeof(mic1_samples), &bytes_read1, portMAX_DELAY);
  i2s_read(I2S_NUM_1, mic2_samples, sizeof(mic2_samples), &bytes_read2, portMAX_DELAY);

  int sample_count = min(bytes_read1, bytes_read2) / sizeof(int16_t);
  
  for(int i = 0; i < sample_count; i++) {
    float primary = mic1_samples[i] / 32768.0f;
    float reference = mic2_samples[i] / 32768.0f;
    
    float cleaned = lms_filter(primary, reference);
    iterationCount++;
    
    mic1_samples[i] = (int16_t)(32767.0f * tanh(cleaned));
  }

  // Serial output for real-time plotting
  if (millis() - lastSerialOutput > SERIAL_OUTPUT_INTERVAL_MS) {
    float current_rmse = calculate_rmse();
    
    // CSV format for Python parsing
    Serial.print(millis());
    Serial.print(",");
    Serial.print(iterationCount);
    Serial.print(",");
    Serial.println(current_rmse, 6); // 6 decimal places
    
    lastSerialOutput = millis();
  }
   size_t bytes_written;
  i2s_write(I2S_NUM_0, mic1_samples, bytes_read1, &bytes_written, portMAX_DELAY);
}
