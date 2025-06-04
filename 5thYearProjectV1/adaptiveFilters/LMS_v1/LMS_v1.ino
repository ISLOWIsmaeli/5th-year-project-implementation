#include <driver/i2s.h>
#include <math.h>

// Pins & Config
#define PRIMARY_SCK 14
#define PRIMARY_WS 15
#define PRIMARY_SD 32
#define AMP_DOUT 22
#define SECONDARY_SCK 26
#define SECONDARY_WS 25
#define SECONDARY_SD 35

// Enhanced Buffer Configuration
#define SAMPLE_BUFFER_SIZE 256  // Increased buffer size for stability
#define BYTES_TO_READ (SAMPLE_BUFFER_SIZE * sizeof(int16_t))

// ANC Parameters
#define FILTER_TAPS 32
#define MU 0.00001f
#define RMSE_WINDOW 100
#define SERIAL_OUTPUT_INTERVAL_MS 50  // Update plot every 50ms

// Audio Enhancement Parameters
#define OUTPUT_GAIN 2.5f        // Increased output gain for louder sound (1.0-4.0)
#define NOISE_GATE_THRESHOLD 100 // Minimum signal level to process
#define DYNAMIC_RANGE_COMPRESSION 0.7f // Compression ratio (0.5-1.0)
#define DC_OFFSET_ALPHA 0.001f  // DC offset removal filter coefficient

// LMS Filter Variables
float w[FILTER_TAPS] = {0};
float x_history[FILTER_TAPS] = {0};

// Error tracking
float error_buffer[RMSE_WINDOW] = {0};
int error_index = 0;
unsigned long lastSerialOutput = 0;
unsigned long iterationCount = 0;

// DC offset removal variables
float dc_offset1 = 0;
float dc_offset2 = 0;

void setup() {
  Serial.begin(115200);
  
  // Enhanced Primary I2S configuration
  i2s_config_t primary_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate = 48000,  // Higher sample rate for better quality
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,    // More buffers for stability
    .dma_buf_len = 512,    // Larger buffers reduce glitches
    .use_apll = true,      // Better clock precision
    .tx_desc_auto_clear = true
  };
  i2s_driver_install(I2S_NUM_0, &primary_config, 0, NULL);
  
  i2s_pin_config_t primary_pins = {
    .bck_io_num = PRIMARY_SCK,
    .ws_io_num = PRIMARY_WS,
    .data_out_num = AMP_DOUT,
    .data_in_num = PRIMARY_SD
  };
  i2s_set_pin(I2S_NUM_0, &primary_pins);

  // Enhanced Secondary I2S configuration
  i2s_config_t secondary_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 48000,  // Match primary sample rate
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,    // More buffers for stability
    .dma_buf_len = 512,
    .use_apll = true       // Better clock precision
  };
  i2s_driver_install(I2S_NUM_1, &secondary_config, 0, NULL);
  
  i2s_pin_config_t secondary_pins = {
    .bck_io_num = SECONDARY_SCK,
    .ws_io_num = SECONDARY_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = SECONDARY_SD,
  };
  i2s_set_pin(I2S_NUM_1, &secondary_pins);

  // Initialize I2S buffers
  i2s_zero_dma_buffer(I2S_NUM_0);
  i2s_zero_dma_buffer(I2S_NUM_1);
  i2s_start(I2S_NUM_0);
  i2s_start(I2S_NUM_1);

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

// Enhanced soft clipping function for natural distortion prevention
float soft_clip(float sample) {
  return tanh(sample * DYNAMIC_RANGE_COMPRESSION) / DYNAMIC_RANGE_COMPRESSION;
}

void loop() {
  int16_t mic1_samples[SAMPLE_BUFFER_SIZE];
  int16_t mic2_samples[SAMPLE_BUFFER_SIZE];
  size_t bytes_read1, bytes_read2;

  // Read samples with timeout for better stability
  if (i2s_read(I2S_NUM_0, mic1_samples, BYTES_TO_READ, &bytes_read1, pdMS_TO_TICKS(20)) != ESP_OK ||
      i2s_read(I2S_NUM_1, mic2_samples, BYTES_TO_READ, &bytes_read2, pdMS_TO_TICKS(20)) != ESP_OK ||
      bytes_read1 == 0 || bytes_read2 == 0) {
    return;
  }

  int sample_count = min(bytes_read1, bytes_read2) / sizeof(int16_t);
  
  for(int i = 0; i < sample_count; i++) {
    // Convert to floating point (-1.0 to 1.0 range)
    float primary = mic1_samples[i] / 32768.0f;
    float reference = mic2_samples[i] / 32768.0f;
    
    // Remove DC offset (high-pass filter) for cleaner audio
    dc_offset1 = primary * DC_OFFSET_ALPHA + dc_offset1 * (1.0f - DC_OFFSET_ALPHA);
    dc_offset2 = reference * DC_OFFSET_ALPHA + dc_offset2 * (1.0f - DC_OFFSET_ALPHA);
    primary -= dc_offset1;
    reference -= dc_offset2;
    
    // Apply LMS adaptive filtering for noise cancellation
    float cleaned = lms_filter(primary, reference);
    iterationCount++;
    
    // Apply noise gate to remove low-level noise
    if (fabs(cleaned) < (NOISE_GATE_THRESHOLD / 32768.0f)) {
      cleaned *= 0.1f; // Reduce instead of completely muting
    }
    
    // Apply dynamic range compression and soft clipping
    cleaned = soft_clip(cleaned);
    
    // Apply output gain for louder sound and convert back to 16-bit
    mic1_samples[i] = constrain((int16_t)(cleaned * OUTPUT_GAIN * 32767.0f), -32768, 32767);
  }

  // Serial output for real-time plotting (MATLAB)
  if (millis() - lastSerialOutput > SERIAL_OUTPUT_INTERVAL_MS) {
    float current_rmse = calculate_rmse();
    
    // CSV format for Python/MATLAB parsing
    Serial.print(millis());
    Serial.print(",");
    Serial.print(iterationCount);
    Serial.print(",");
    Serial.println(current_rmse, 6); // 6 decimal places
    lastSerialOutput = millis();
  }
  
  // Write processed audio to output with timeout
  size_t bytes_written;
  i2s_write(I2S_NUM_0, mic1_samples, sample_count * sizeof(int16_t), &bytes_written, pdMS_TO_TICKS(20));
}
