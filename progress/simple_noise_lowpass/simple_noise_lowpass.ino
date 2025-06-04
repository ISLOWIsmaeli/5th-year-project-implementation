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

// Sensitivity and gain settings
#define MIC_GAIN 3.0          // Microphone input amplification (1.0-10.0)
#define OUTPUT_GAIN 2.5       // Speaker output amplification (1.0-5.0)
#define NOISE_GATE 50         // Minimum signal level to process (reduces background noise)

// Low-pass filter variables
float alpha = 0.1;            // Low-pass filter coefficient (0.05-0.3 for voice filtering)
int16_t prev_output_mic1 = 0; // Previous output for mic1 filter
int16_t prev_output_mic2 = 0; // Previous output for mic2 filter
int16_t prev_output_mixed = 0; // Previous output for mixed signal filter

// Low-pass filter function
int16_t low_pass_filter(int16_t input_sample, int16_t* prev_output) {
    int16_t output_sample = (alpha * input_sample) + ((1.0 - alpha) * (*prev_output));
    *prev_output = output_sample;  // Update previous output
    return output_sample;
}

void setup() {
    Serial.begin(115200);
    
    // Primary I2S configuration (Mic1 + Amplifier output)
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
    
    // Secondary I2S configuration (Mic2 input only)
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
    
    // Initialize I2S buffers and start
    i2s_zero_dma_buffer(I2S_NUM_0);
    i2s_zero_dma_buffer(I2S_NUM_1);
    i2s_start(I2S_NUM_0);
    i2s_start(I2S_NUM_1);
    
    Serial.println("Dual microphone system with low-pass filter initialized");
    Serial.println("Filter cutoff frequency: ~700Hz (suitable for voice filtering)");
}

void loop() {
    int16_t mic1_samples[SAMPLE_BUFFER_SIZE];
    int16_t mic2_samples[SAMPLE_BUFFER_SIZE];
    size_t bytes_read1, bytes_read2;
    
    // Read from both microphones
    if (i2s_read(I2S_NUM_0, mic1_samples, BYTES_TO_READ, &bytes_read1, pdMS_TO_TICKS(10)) != ESP_OK ||
        i2s_read(I2S_NUM_1, mic2_samples, BYTES_TO_READ, &bytes_read2, pdMS_TO_TICKS(10)) != ESP_OK ||
        bytes_read1 == 0 || bytes_read2 == 0) {
        return;
    }
    
    int sample_count = min(bytes_read1, bytes_read2) / sizeof(int16_t);
    
    for(int i = 0; i < sample_count; i++) {
        // Apply input gain to individual microphones
        int32_t sample1 = (int32_t)(mic1_samples[i] * MIC_GAIN);
        int32_t sample2 = (int32_t)(mic2_samples[i] * MIC_GAIN);
        
        // Clamp amplified inputs to prevent overflow before filtering
        sample1 = constrain(sample1, -32768, 32767);
        sample2 = constrain(sample2, -32768, 32767);
        
        // Apply low-pass filter to each microphone individually
        int16_t filtered_sample1 = low_pass_filter((int16_t)sample1, &prev_output_mic1);
        int16_t filtered_sample2 = low_pass_filter((int16_t)sample2, &prev_output_mic2);
        
        // Mix the filtered samples
        int32_t mixed = (filtered_sample1 + filtered_sample2) / 2;
        
        // Apply noise gate - ignore very quiet signals
        if (abs(mixed) < NOISE_GATE) {
            mixed = 0;
        }
        
        // Apply final low-pass filter to mixed signal for additional smoothing
        int16_t final_filtered = low_pass_filter((int16_t)mixed, &prev_output_mixed);
        
        // Apply output gain for louder speaker output
        int32_t output = (int32_t)(final_filtered * OUTPUT_GAIN);
        
        // Final clamp to prevent distortion
        mic1_samples[i] = constrain(output, -32768, 32767);
    }
    
    // Write processed audio to amplifier
    size_t bytes_written;
    i2s_write(I2S_NUM_0, mic1_samples, sample_count * sizeof(int16_t), &bytes_written, pdMS_TO_TICKS(10));
}
