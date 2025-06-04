#include <driver/i2s.h>
#include <math.h>

// Primary I2S Bus (Mic1 + Amp)
#define PRIMARY_SCK 14
#define PRIMARY_WS 15
#define PRIMARY_SD 32
#define AMP_DOUT 22

// Secondary I2S Bus (Mic2)
#define SECONDARY_SCK 26
#define SECONDARY_WS 25
#define SECONDARY_SD 35

#define SAMPLE_BUFFER_SIZE 256  // Increased buffer size for stability
#define BYTES_TO_READ (SAMPLE_BUFFER_SIZE * sizeof(int16_t))

// Audio processing parameters
#define MIC_GAIN 2.5f           // Initial microphone gain (1.0-4.0)
#define OUTPUT_GAIN 1.8f        // Output gain (1.0-3.0)
#define NOISE_GATE_THRESHOLD 0 // Minimum signal level to process
#define DYNAMIC_RANGE_COMPRESSION 0.8f // Compression ratio (0.5-1.0)
#define DC_OFFSET_ALPHA 0.001f  // DC offset removal filter coefficient

// DC offset removal variables
float dc_offset1 = 0;
float dc_offset2 = 0;

void setup() {
  Serial.begin(115200);
  
  // Primary I2S configuration (optimized for quality)
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
  
  // Secondary I2S configuration
  i2s_config_t secondary_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 48000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 512,
    .use_apll = true
  };
  
  i2s_driver_install(I2S_NUM_1, &secondary_config, 0, NULL);
  
  i2s_pin_config_t secondary_pins = {
    .bck_io_num = SECONDARY_SCK,
    .ws_io_num = SECONDARY_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = SECONDARY_SD,
  };
  i2s_set_pin(I2S_NUM_1, &secondary_pins);
  
  // Initialize I2S
  i2s_zero_dma_buffer(I2S_NUM_0);
  i2s_zero_dma_buffer(I2S_NUM_1);
  i2s_start(I2S_NUM_0);
  i2s_start(I2S_NUM_1);
}

// Soft clipping function for natural distortion prevention
float soft_clip(float sample) {
  return tanh(sample * DYNAMIC_RANGE_COMPRESSION) / DYNAMIC_RANGE_COMPRESSION;
}

void loop() {
  int16_t mic1_samples[SAMPLE_BUFFER_SIZE];
  int16_t mic2_samples[SAMPLE_BUFFER_SIZE];
  size_t bytes_read1, bytes_read2;
  
  // Read samples with timeout
  if (i2s_read(I2S_NUM_0, mic1_samples, BYTES_TO_READ, &bytes_read1, pdMS_TO_TICKS(20)) != ESP_OK ||
      i2s_read(I2S_NUM_1, mic2_samples, BYTES_TO_READ, &bytes_read2, pdMS_TO_TICKS(20)) != ESP_OK ||
      bytes_read1 == 0 || bytes_read2 == 0) {
    return;
  }
  
  int sample_count = min(bytes_read1, bytes_read2) / sizeof(int16_t);
  
  for(int i = 0; i < sample_count; i++) {
    // Convert to floating point (-1.0 to 1.0 range)
    float sample1 = mic1_samples[i] / 32768.0f;
    float sample2 = mic2_samples[i] / 32768.0f;
    
    // Remove DC offset (high-pass filter)
    dc_offset1 = sample1 * DC_OFFSET_ALPHA + dc_offset1 * (1.0f - DC_OFFSET_ALPHA);
    dc_offset2 = sample2 * DC_OFFSET_ALPHA + dc_offset2 * (1.0f - DC_OFFSET_ALPHA);
    sample1 -= dc_offset1;
    sample2 -= dc_offset2;
    
    // Mix the samples with gain
    float mixed = (sample1 + sample2) * 0.5f * MIC_GAIN;
    
    // Apply noise gate
    if (fabs(mixed) < (NOISE_GATE_THRESHOLD / 32768.0f)) {
      mixed = 0;
    }
    
    // Apply dynamic range compression and soft clipping
    mixed = soft_clip(mixed);
    
    // Convert back to 16-bit integer with output gain
    mic1_samples[i] = constrain((int16_t)(mixed * OUTPUT_GAIN * 32767.0f), -32768, 32767);
  }
  
  // Write processed audio to output
  size_t bytes_written;
  i2s_write(I2S_NUM_0, mic1_samples, sample_count * sizeof(int16_t), &bytes_written, pdMS_TO_TICKS(20));
}
