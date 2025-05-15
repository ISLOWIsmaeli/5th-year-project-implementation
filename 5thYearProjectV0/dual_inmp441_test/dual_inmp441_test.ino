#include <driver/i2s.h>

// Hardware Connections (keep your existing wiring)
#define PRIMARY_MIC_SD   32
#define REFERENCE_MIC_SD 33
#define I2S_SCK          14
#define I2S_WS           15
#define AMP_DOUT         25

// Audio Configuration
#define SAMPLE_RATE     16000
#define BUFFER_SIZE      256
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT

void setup() {
  Serial.begin(115200);
  
  // Initialize I2S for both microphones
  setup_i2s();
  
  Serial.println("Dual Microphone Test Started");
  Serial.println("Primary mic → Left channel");
  Serial.println("Reference mic → Right channel");
}

void loop() {
  int16_t mic_samples[BUFFER_SIZE * 2]; // Stereo buffer (L=primary, R=reference)
  size_t bytes_read;
  
  // Read interleaved stereo data (alternating L/R samples)
  i2s_read(I2S_NUM_0, mic_samples, sizeof(mic_samples), &bytes_read, portMAX_DELAY);
  
  // Send raw mic data to amplifier (mix both channels)
  size_t bytes_written;
  i2s_write(I2S_NUM_0, mic_samples, bytes_read, &bytes_written, portMAX_DELAY);
  
  // Print sample values for debugging (first 4 samples)
  Serial.printf("L:%6d R:%6d | L:%6d R:%6d\n", 
               mic_samples[0], mic_samples[1],  // First L/R pair
               mic_samples[2], mic_samples[3]); // Second L/R pair
  delay(100); // Reduce serial spam
}

void setup_i2s() {
  // I2S configuration for stereo input
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = BITS_PER_SAMPLE,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // Stereo
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = BUFFER_SIZE,
    .use_apll = false
  };

  // Install driver
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);

  // Pin configuration (both mics on same I2S peripheral)
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = AMP_DOUT,
    .data_in_num = PRIMARY_MIC_SD  // WS will automatically handle both mics
  };
  i2s_set_pin(I2S_NUM_0, &pin_config);
  
  // Special patch for dual-mic on single I2S
  REG_SET_BIT(I2S_PIN_CTRL_REG, BIT(9)); // Enable WS signal output
  gpio_matrix_out(REFERENCE_MIC_SD, I2S0O_DATA_OUT0_IDX, false, false);
}
