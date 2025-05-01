#include <driver/i2s.h>

// I2S pins
#define I2S_SCK 14
#define I2S_WS 15
#define I2S_SD 32

// I2S configuration
#define SAMPLE_RATE 16000
#define SAMPLE_BITS 16
#define BUFFER_SIZE 1024

void setup() {
  Serial.begin(115200);
  
  // Set up I2S
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    // MASTER: ESP32 controls timing
    // RX: Receive mode (getting data from mic)
//    I2S_MODE_MASTER | I2S_MODE_SLAVE  // Clock control
//    I2S_MODE_TX | I2S_MODE_RX          // Data direction
//    I2S_MODE_DAC_BUILT_IN              // For internal DAC
//    I2S_MODE_ADC_BUILT_IN              // For internal ADC
//    I2S_MODE_PDM                       // For PDM microphones
    .sample_rate = SAMPLE_RATE,
    // As discussed - samples per second (16,000)
    .bits_per_sample = (i2s_bits_per_sample_t)SAMPLE_BITS,
    // As discussed - samples per second (16,000)
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
//        I2S_CHANNEL_FMT_ONLY_RIGHT        // Mono right
//        I2S_CHANNEL_FMT_ONLY_LEFT         // Your setting (mono left)
//        I2S_CHANNEL_FMT_RIGHT_LEFT        // Stereo
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    // Standard I2S protocol format
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
     // Interrupt priority level
//      ESP_INTR_FLAG_LEVEL1  // Low priority (your setting)
//      ESP_INTR_FLAG_LEVEL2  // Medium
//      ESP_INTR_FLAG_LEVEL3  // High
//      ESP_INTR_FLAG_IRAM    // IRAM-safe
    .dma_buf_count = 8,
    // Number of DMA buffers (more buffers = more stable but higher latency)
    .dma_buf_len = BUFFER_SIZE,
    // Size of each buffer in samples
    .use_apll = false
    // Don't use Audio PLL for clock (simpler setup)
  };

//  typedef struct {
//    int bck_io_num;     // Bit clock
//    int ws_io_num;      // Word select
//    int data_out_num;   // DATA out (even if unused)
//    int data_in_num;    // DATA in
//} i2s_pin_config_t;

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  // Install and start I2S driver
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  
  Serial.println("I2S setup complete");
}

void loop() {
  int16_t samples[BUFFER_SIZE];
  size_t bytes_read;
  
  // Read audio data from I2S
  i2s_read(I2S_NUM_0, &samples, BUFFER_SIZE * sizeof(int16_t), &bytes_read, portMAX_DELAY);
  
  // Print the first sample (for demonstration)
  Serial.println(samples[0]);
  
  delay(100); // Small delay to prevent flooding the serial monitor
}
