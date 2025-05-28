#include <driver/i2s.h>

int bytes_read;
size_t bytesIn, bytesWritten;

#define buffer_Len 64
int16_t i2s_Buffer[buffer_Len];
// 
// INMP441 I2S microphone connections
//
#define I2S_WS_MIC 25
#define I2S_SD_MIC 32
#define I2S_SCK_MIC 33
//
// MAX98357A amplifier connections
//
#define I2S_WS_AMP 21
#define I2S_SD_AMP 22
#define I2S_SCK_AMP 23
#define I2S_PORT0 I2S_NUM_0             // I2S0
#define I2S_PORT1 I2S_NUM_1             // I2S1
//
// I2S microphone pin configuration
//
void i2s_setpin_MIC() {
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK_MIC,
    .ws_io_num = I2S_WS_MIC,
    .data_out_num = I2S_PIN_NO_CHANGE,         // Not used (output only)
    .data_in_num = I2S_SD_MIC
  };
  i2s_set_pin(I2S_PORT0, &pin_config);
}
//
// I2S amplifier pin configuration
//
void i2s_setpin_AMP(){
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK_AMP,
    .ws_io_num = I2S_WS_AMP,
    .data_out_num = I2S_SD_AMP,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  i2s_set_pin(I2S_PORT1, &pin_config);
}
//
// Configure I2S0 for microphone
//
void i2s_install_MIC() {
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 44100,
    .bits_per_sample = i2s_bits_per_sample_t(16),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
   .dma_buf_len = buffer_Len,
    .use_apll = false
  };
  i2s_driver_install(I2S_PORT0, &i2s_config, 0, NULL);
}
//
// Configure I2S1 for amplifier
//
void i2s_install_AMP() {
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = i2s_bits_per_sample_t(16),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = buffer_Len,
    .use_apll = false
  };
  i2s_driver_install(I2S_PORT1, &i2s_config, 0, NULL);
}
void setup() {
  Serial.begin(115200);
  i2s_install_MIC();
  i2s_install_AMP();
  i2s_setpin_MIC();
  i2s_setpin_AMP();
  i2s_start(I2S_PORT0);
  i2s_start(I2S_PORT1);
}
//
// Read audio samples into i2s_Buffer from the microphone
// send the captured data to the amplifier and then to the
// speaker
//
//void loop(){
//  bytesIn = 0;
//  bytes_read = i2s_read(I2S_PORT0,&i2s_Buffer,buffer_Len,&bytesIn,portMAX_DELAY);
//  if(bytesIn > 0)
//  {
//    i2s_write(I2S_PORT1,&i2s_Buffer,bytesIn,&bytesWritten,portMAX_DELAY); 
//  }
//}

// to amplify the output by a scale factor of 3
void loop() {
   bytesIn = 0;
   bytes_read = i2s_read(I2S_PORT0,&i2s_Buffer,buffer_Len,&bytesIn,portMAX_DELAY);
   if(bytesIn > 0){
      for(int i=0; i < bytesIn; i++){
        i2s_Buffer[i]=3*i2s_Buffer[i];
       }
      i2s_write(I2S_PORT1,&i2s_Buffer,bytesIn,&bytesWritten,portMAX_DELAY); 
  }
}
