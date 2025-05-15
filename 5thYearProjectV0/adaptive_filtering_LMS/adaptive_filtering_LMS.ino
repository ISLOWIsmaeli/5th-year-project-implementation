#include <driver/i2s.h>
#include <math.h>

// Hardware Connections
#define PRIMARY_MIC_SD   32
#define REFERENCE_MIC_SD 33
#define I2S_SCK          14
#define I2S_WS           15
#define AMP_DOUT         25

// LMS Algorithm Parameters
#define FILTER_LENGTH    32     // Tap length of adaptive filter
#define MU              0.01f   // Convergence rate (0.001 to 0.1)
float w[FILTER_LENGTH] = {0};   // Adaptive filter weights

// Audio Configuration
#define SAMPLE_RATE     16000
#define BUFFER_SIZE      256
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT

// System monitoring
unsigned long last_debug_time = 0;
QueueHandle_t i2s_event_queue;

void setup() {
  Serial.begin(115200);
  
  // Force GPIO states
  pinMode(I2S_SCK, OUTPUT);
  pinMode(I2S_WS, OUTPUT);
  pinMode(AMP_DOUT, OUTPUT);
  digitalWrite(I2S_SCK, LOW);
  digitalWrite(I2S_WS, LOW);
  digitalWrite(AMP_DOUT, LOW);

  // Initialize I2S with diagnostic event queue
  setup_i2s();
  
  // Test tone generator (comment out after verification)
  //generate_test_tone();
  
  Serial.println("System Ready. Sample rate: " + String(SAMPLE_RATE) + "Hz");
}

void loop() {
  // 1. Read microphone data
  int16_t primary_samples[BUFFER_SIZE];
  int16_t reference_samples[BUFFER_SIZE];
  size_t bytes_read_prim, bytes_read_ref;
  
  i2s_read(I2S_NUM_0, primary_samples, sizeof(primary_samples), &bytes_read_prim, 0);
  i2s_read(I2S_NUM_1, reference_samples, sizeof(reference_samples), &bytes_read_ref, 0);

  // 2. Process audio with LMS noise cancellation
  int sample_count = bytes_read_prim / sizeof(int16_t);
  int16_t output_samples[sample_count];
  
  for(int i = 0; i < sample_count; i++) {
    float cleaned = lms_filter(primary_samples[i], reference_samples[i]);
    // Apply 8x gain (18dB) with safe clipping
    output_samples[i] = (int16_t)constrain(cleaned * 8, -32768, 32767);
  }

  // 3. Send to amplifier
  size_t bytes_written;
  esp_err_t err = i2s_write(I2S_NUM_0, output_samples, bytes_read_prim, &bytes_written, 0);
  if(err != ESP_OK) {
    Serial.println("I2S Write Error: " + String(err));
  }

  // 4. System monitoring
  if(millis() - last_debug_time > 1000) {
    debug_system_status();
    last_debug_time = millis();
  }
}

// Audio Processing Functions
float lms_filter(int16_t primary, int16_t reference) {
  static float x_history[FILTER_LENGTH] = {0};
  float y = 0;
  
  // Shift history buffer
  for(int i = FILTER_LENGTH-1; i > 0; i--) {
    x_history[i] = x_history[i-1];
  }
  x_history[0] = (float)reference;
  
  // Calculate filter output
  for(int i = 0; i < FILTER_LENGTH; i++) {
    y += w[i] * x_history[i];
  }
  
  // Error calculation
  float error = (float)primary - y;
  
  // Update weights
  for(int i = 0; i < FILTER_LENGTH; i++) {
    w[i] += MU * error * x_history[i];
  }
  
  return error;
}

// Hardware Setup
void setup_i2s() {
  // I2S configuration for both TX and RX
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = BITS_PER_SAMPLE,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = BUFFER_SIZE,
    .use_apll = true,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  // Install drivers with event queue
  i2s_driver_install(I2S_NUM_0, &i2s_config, 4, &i2s_event_queue);
  i2s_driver_install(I2S_NUM_1, &i2s_config, 0, NULL);

  // Primary mic (I2S_NUM_0) + amplifier output
  i2s_pin_config_t pin_config_prim = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = AMP_DOUT,
    .data_in_num = PRIMARY_MIC_SD
  };
  i2s_set_pin(I2S_NUM_0, &pin_config_prim);

  // Reference mic (I2S_NUM_1)
  i2s_pin_config_t pin_config_ref = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = REFERENCE_MIC_SD
  };
  i2s_set_pin(I2S_NUM_1, &pin_config_ref);

  // Set clock for clean output
  i2s_set_clk(I2S_NUM_0, SAMPLE_RATE, BITS_PER_SAMPLE, I2S_CHANNEL_MONO);
}

// Diagnostics
void debug_system_status() {
  // I2S event monitoring
  i2s_event_t event;
  while(xQueueReceive(i2s_event_queue, &event, 0)) {
    if(event.type == I2S_EVENT_TX_DONE) Serial.println("TX Active");
    if(event.type == I2S_EVENT_RX_DONE) Serial.println("RX Active");
  }

  // DMA buffer status
//  Serial.printf("DMA Status - Free: %d, Used: %d, Min Free: %d\n",i2s_get_free_tx_queue(I2S_NUM_0),i2s_get_used_tx_queue(I2S_NUM_0),i2s_get_tx_queue_space(I2S_NUM_0));

  // Filter weight monitoring
  float w_sum = 0;
  for(int i=0; i<FILTER_LENGTH; i++) w_sum += abs(w[i]);
  Serial.println("Avg Filter Weight: " + String(w_sum/FILTER_LENGTH, 6));
}

// Initial test function (uncomment to verify hardware)
void generate_test_tone() {
  int16_t test_samples[BUFFER_SIZE];
  for(int i=0; i<BUFFER_SIZE; i++) {
    test_samples[i] = (int16_t)(32767 * sin(2*PI*1000*i/SAMPLE_RATE));
  }
  
  size_t bytes_written;
  for(int i=0; i<10; i++) { // Send 10 buffers of test tone
    i2s_write(I2S_NUM_0, test_samples, sizeof(test_samples), &bytes_written, portMAX_DELAY);
  }
  Serial.println("Test tone generated");
}
