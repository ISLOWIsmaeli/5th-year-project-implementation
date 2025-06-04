#include <driver/i2s.h>

// Primary I2S Bus (Mic1 + Amp)
#define PRIMARY_SCK 14
#define PRIMARY_WS 15
#define PRIMARY_SD 32
#define AMP_DOUT 22

// Secondary I2S Bus (Mic2)
#define SECONDARY_SCK 26
#define SECONDARY_WS 25
#define SECONDARY_SD 35

#define SAMPLE_BUFFER_SIZE 128
#define BYTES_TO_READ (SAMPLE_BUFFER_SIZE * sizeof(int16_t))

void setup() {
  Serial.begin(115200);
  
  i2s_config_t primary_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = true,
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
  
  i2s_config_t secondary_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
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
  
  i2s_zero_dma_buffer(I2S_NUM_0);
  i2s_zero_dma_buffer(I2S_NUM_1);
  i2s_start(I2S_NUM_0);
  i2s_start(I2S_NUM_1);
}

void loop() {
  int16_t mic1_samples[SAMPLE_BUFFER_SIZE];
  int16_t mic2_samples[SAMPLE_BUFFER_SIZE];
  size_t bytes_read1, bytes_read2;
  
  if (i2s_read(I2S_NUM_0, mic1_samples, BYTES_TO_READ, &bytes_read1, pdMS_TO_TICKS(10)) != ESP_OK ||
      i2s_read(I2S_NUM_1, mic2_samples, BYTES_TO_READ, &bytes_read2, pdMS_TO_TICKS(10)) != ESP_OK ||
      bytes_read1 == 0 || bytes_read2 == 0) {
    return;
  }
  
  int sample_count = min(bytes_read1, bytes_read2) / sizeof(int16_t);
  
  for(int i = 0; i < sample_count; i++) {
    int32_t mixed = ((int32_t)mic1_samples[i] + mic2_samples[i]) / 2;
    mic1_samples[i] = constrain(mixed, -32768, 32767);
  }
  
  size_t bytes_written;
  i2s_write(I2S_NUM_0, mic1_samples, sample_count * sizeof(int16_t), &bytes_written, pdMS_TO_TICKS(10));
}
