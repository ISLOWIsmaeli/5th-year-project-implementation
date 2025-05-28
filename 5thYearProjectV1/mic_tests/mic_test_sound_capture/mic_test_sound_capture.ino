#include <driver/i2s.h>

int16_t i, samples_read;
int bytes_read;
size_t bytesIn;

#define buffer_Len 64

int16_t i2s_Buffer[buffer_Len];
float mean_samples;
 
// INMP441 I2S microphone connections

#define I2S_WS 25    // WS pin
#define I2S_SD 32    // SD pin
#define I2S_SCK 33    // SCK pin
 
#define I2S_PORT I2S_NUM_0            // Use I2S0

// I2S pin configuration

void i2s_setpin() {
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,         // Not used (output only)
    .data_in_num = I2S_SD
  };
  i2s_set_pin(I2S_PORT, &pin_config);
 }
 
// Configure I2S //
void i2s_install(){
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 44100,
    .bits_per_sample = i2s_bits_per_sample_t(16),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format=i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = buffer_Len,
    .use_apll = false
  };
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

void setup() 
{
  Serial.begin(115200);   // Serial Monitor
  i2s_install();    // Install i2s
  i2s_setpin();    // COnfigure pins
  i2s_start(I2S_PORT);    // Start i2s
  delay(500);     // Small delay
 }
 //
 // Read audio samples into i2s_Buffer, find the mean and send to plotter.
 // bytesIn is the number of bytes read, i2s_Buffer stores the samples read
 // buffer_Len is the length of the buffer
 //
 void loop() 
{
  bytesIn = 0;
  bytes_read = i2s_read(I2S_PORT,&i2s_Buffer,buffer_Len,&bytesIn,portMAX_DELAY);
  Serial.println(bytes_read);
  Serial.println("----------");
  samples_read = bytesIn / 8;
  if(samples_read > 0)
  {
    mean_samples = 0;
    for(i=0; i < samples_read; i++)
    {
      mean_samples = mean_samples + i2s_Buffer[i];
    }
    mean_samples = mean_samples / samples_read;
    Serial.println(mean_samples);  // Send to Serial Monitor (plotter)
  }
 }
