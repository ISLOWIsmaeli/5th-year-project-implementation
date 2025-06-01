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

// Buffer size - reduced for lower latency
#define SAMPLE_BUFFER_SIZE 128
#define BYTES_TO_READ (SAMPLE_BUFFER_SIZE * sizeof(int16_t))

void setup() {
  Serial.begin(115200);
  
  // Primary I2S Config (TX to amp + RX from mic1)
  i2s_config_t primary_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,        // Increased buffer count
    .dma_buf_len = 256,        // Reduced buffer length for lower latency
    .use_apll = true,
    .tx_desc_auto_clear = true // Clear TX descriptor automatically
  };
  
  i2s_driver_install(I2S_NUM_0, &primary_config, 0, NULL);
  
  // Primary pin config
  i2s_pin_config_t primary_pins = {
    .bck_io_num = PRIMARY_SCK,
    .ws_io_num = PRIMARY_WS,
    .data_out_num = AMP_DOUT,
    .data_in_num = PRIMARY_SD
  };
  i2s_set_pin(I2S_NUM_0, &primary_pins);
  
  // Secondary I2S Config (RX from mic2 only)
  i2s_config_t secondary_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,        // Increased buffer count
    .dma_buf_len = 256,        // Consistent with primary
    .use_apll = true           // Changed to true for better sync
  };
  
  i2s_driver_install(I2S_NUM_1, &secondary_config, 0, NULL);
  
  // Secondary pin config
  i2s_pin_config_t secondary_pins = {
    .bck_io_num = SECONDARY_SCK,
    .ws_io_num = SECONDARY_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = SECONDARY_SD,
  };
  i2s_set_pin(I2S_NUM_1, &secondary_pins);
  
  // Clear any existing data in the I2S buffers
  i2s_zero_dma_buffer(I2S_NUM_0);
  i2s_zero_dma_buffer(I2S_NUM_1);
  
  // Start both I2S peripherals
  i2s_start(I2S_NUM_0);
  i2s_start(I2S_NUM_1);
  
  Serial.println("I2S initialized successfully");
}

void loop() {
  int16_t mic1_samples[SAMPLE_BUFFER_SIZE];
  int16_t mic2_samples[SAMPLE_BUFFER_SIZE];
  size_t bytes_read1, bytes_read2;
  
  // Clear buffers before reading
  memset(mic1_samples, 0, sizeof(mic1_samples));
  memset(mic2_samples, 0, sizeof(mic2_samples));
  
  // Read from both mics with timeout
  esp_err_t err1 = i2s_read(I2S_NUM_0, mic1_samples, BYTES_TO_READ, &bytes_read1, pdMS_TO_TICKS(10));
  esp_err_t err2 = i2s_read(I2S_NUM_1, mic2_samples, BYTES_TO_READ, &bytes_read2, pdMS_TO_TICKS(10));
  
  if (err1 != ESP_OK || err2 != ESP_OK) {
    Serial.printf("Read error: mic1=%d, mic2=%d\n", err1, err2);
    return;
  }
  
  // Ensure we have valid data
  if (bytes_read1 == 0 || bytes_read2 == 0) {
    Serial.println("No data read from microphones");
    return;
  }
  
  // Calculate actual sample count
  int sample_count1 = bytes_read1 / sizeof(int16_t);
  int sample_count2 = bytes_read2 / sizeof(int16_t);
  int sample_count = min(sample_count1, sample_count2);
  
  if (sample_count <= 0) {
    return;
  }
  
  // Mix samples with proper scaling and overflow protection
  for(int i = 0; i < sample_count; i++) {
    // Convert to 32-bit for calculation
    int32_t sample1 = mic1_samples[i];
    int32_t sample2 = mic2_samples[i];
    
    // Simple average mixing
    int32_t mixed = (sample1 + sample2) / 2;
    
    // Clamp to 16-bit range
    if (mixed > 32767) mixed = 32767;
    if (mixed < -32768) mixed = -32768;
    
    mic1_samples[i] = (int16_t)mixed;
  }
  
  // Output mixed audio - use actual bytes read for consistency
  size_t bytes_to_write = sample_count * sizeof(int16_t);
  size_t bytes_written;
  
  esp_err_t write_err = i2s_write(I2S_NUM_0, mic1_samples, bytes_to_write, &bytes_written, pdMS_TO_TICKS(10));
  
  if (write_err != ESP_OK) {
    Serial.printf("Write error: %d\n", write_err);
  }
  
  // Optional: Print debug info periodically
  static int debug_counter = 0;
  if (++debug_counter % 1000 == 0) {
    Serial.printf("Samples: mic1=%d, mic2=%d, mixed=%d, written=%d\n", 
                  sample_count1, sample_count2, sample_count, bytes_written/sizeof(int16_t));
  }
}

/*
 * 
 * # ESP32 I2S Audio Code - Line by Line Explanation

## Header and Definitions Section

```cpp
#include <driver/i2s.h>
```
**What this does:** This line tells the computer "I want to use the I2S audio library." Think of it like importing a toolbox that contains all the tools needed to work with audio on the ESP32.

---

```cpp
// Primary I2S Bus (Mic1 + Amp)
#define PRIMARY_SCK 14
#define PRIMARY_WS 15
#define PRIMARY_SD 32
#define AMP_DOUT 22
```
**What this does:** These lines create shortcuts for pin numbers. Instead of writing "14" everywhere in the code, we can write "PRIMARY_SCK". It's like giving nicknames to the ESP32's pins:
- **SCK** = Clock pin (keeps everything in sync, like a metronome)
- **WS** = Word Select pin (decides left/right channel timing)
- **SD** = Serial Data pin (actual audio data travels here)
- **AMP_DOUT** = Amplifier output pin (sends audio to speakers)

---

```cpp
// Secondary I2S Bus (Mic2)
#define SECONDARY_SCK 26
#define SECONDARY_WS 25
#define SECONDARY_SD 35
```
**What this does:** More pin nicknames for the second microphone. We need separate pins because we're using two microphones at the same time.

---

```cpp
#define SAMPLE_BUFFER_SIZE 128
#define BYTES_TO_READ (SAMPLE_BUFFER_SIZE * sizeof(int16_t))
```
**What this does:** 
- **SAMPLE_BUFFER_SIZE 128:** This says "work with 128 audio samples at a time." Think of it like deciding to process audio in chunks of 128 pieces.
- **BYTES_TO_READ:** This calculates how much computer memory those 128 samples will need. Each sample is an `int16_t` (2 bytes), so 128 × 2 = 256 bytes total.

## Setup Function (Runs Once When ESP32 Starts)

```cpp
void setup() {
  Serial.begin(115200);
```
**What this does:** 
- `void setup()` creates a function that runs once when the ESP32 turns on
- `Serial.begin(115200)` starts the communication between ESP32 and your computer, so you can see debug messages. The number 115200 is the communication speed.

---

```cpp
i2s_config_t primary_config = {
```
**What this does:** Creates a "configuration blueprint" for the first I2S audio system. Think of it like filling out a form with all the audio settings.

---

```cpp
.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
```
**What this does:** Sets the audio mode:
- **I2S_MODE_MASTER:** "I'm the boss, I control the timing"
- **I2S_MODE_RX:** "I can receive audio (from microphone)"
- **I2S_MODE_TX:** "I can transmit audio (to speakers)"
- The `|` symbol means "AND" - so it's doing all three things at once.

---

```cpp
.sample_rate = 44100,
```
**What this does:** Sets audio quality to 44,100 samples per second. This is CD quality - the same rate used for music CDs. Higher numbers = better quality but more processing power needed.

---

```cpp
.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
```
**What this does:** Each audio sample will use 16 bits of data. Think of it like deciding how detailed each audio measurement will be. 16-bit is good quality for most applications.

---

```cpp
.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
```
**What this does:** Only use the left audio channel. Since we're mixing two microphones into one output, we don't need stereo (left + right), just mono (single channel).

---

```cpp
.communication_format = I2S_COMM_FORMAT_STAND_I2S,
```
**What this does:** Use the standard I2S communication protocol. It's like choosing which "language" the audio devices will speak to each other.

---

```cpp
.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
```
**What this does:** Sets interrupt priority. When audio data arrives, this tells the ESP32 "this is moderately important, handle it soon but not urgently."

---

```cpp
.dma_buf_count = 8,
.dma_buf_len = 256,
```
**What this does:** 
- **dma_buf_count = 8:** Create 8 memory buffers for audio data
- **dma_buf_len = 256:** Each buffer holds 256 pieces of data
- Think of it like having 8 buckets, each holding 256 audio samples. While one bucket is being filled, another can be processed.

---

```cpp
.use_apll = true,
.tx_desc_auto_clear = true
```
**What this does:**
- **use_apll = true:** Use a more precise clock for better audio quality
- **tx_desc_auto_clear = true:** Automatically clean up old audio data to prevent interference

---

```cpp
i2s_driver_install(I2S_NUM_0, &primary_config, 0, NULL);
```
**What this does:** Actually starts the first I2S system using all the settings we just configured. `I2S_NUM_0` means "use I2S bus number 0" (the first one).

---

```cpp
i2s_pin_config_t primary_pins = {
  .bck_io_num = PRIMARY_SCK,
  .ws_io_num = PRIMARY_WS,
  .data_out_num = AMP_DOUT,
  .data_in_num = PRIMARY_SD
};
```
**What this does:** Tells the ESP32 which physical pins to use for audio signals. It's like connecting wires:
- Clock signal goes to pin 14
- Timing signal goes to pin 15  
- Audio output goes to pin 22
- Audio input comes from pin 32

---

```cpp
i2s_set_pin(I2S_NUM_0, &primary_pins);
```
**What this does:** Actually connects those pins to the I2S system.

## Secondary I2S Setup (Second Microphone)

The next section does the same thing for the second microphone, but simpler because it only receives audio (no output to speakers):

```cpp
i2s_config_t secondary_config = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),  // Only RX, no TX
  // ... similar settings but no output capability
```

## Buffer Initialization

```cpp
i2s_zero_dma_buffer(I2S_NUM_0);
i2s_zero_dma_buffer(I2S_NUM_1);
```
**What this does:** Clears any leftover audio data from the memory buffers. Like emptying the buckets before starting fresh.

---

```cpp
i2s_start(I2S_NUM_0);
i2s_start(I2S_NUM_1);
```
**What this does:** Actually starts both audio systems running. Now they're ready to record and play audio.

## Main Loop (Runs Continuously)

```cpp
void loop() {
```
**What this does:** Creates a function that runs over and over again, forever. This is where the actual audio processing happens.

---

```cpp
int16_t mic1_samples[SAMPLE_BUFFER_SIZE];
int16_t mic2_samples[SAMPLE_BUFFER_SIZE];
size_t bytes_read1, bytes_read2;
```
**What this does:** Creates storage space:
- **mic1_samples:** Array to hold 128 audio samples from microphone 1
- **mic2_samples:** Array to hold 128 audio samples from microphone 2  
- **bytes_read1, bytes_read2:** Variables to track how much data was actually received

---

```cpp
memset(mic1_samples, 0, sizeof(mic1_samples));
memset(mic2_samples, 0, sizeof(mic2_samples));
```
**What this does:** Fills both arrays with zeros. Like erasing a whiteboard before writing new information.

---

```cpp
esp_err_t err1 = i2s_read(I2S_NUM_0, mic1_samples, BYTES_TO_READ, &bytes_read1, pdMS_TO_TICKS(10));
esp_err_t err2 = i2s_read(I2S_NUM_1, mic2_samples, BYTES_TO_READ, &bytes_read2, pdMS_TO_TICKS(10));
```
**What this does:** 
- Reads audio data from both microphones
- `pdMS_TO_TICKS(10)` means "wait up to 10 milliseconds for data"
- `esp_err_t` stores whether the operation succeeded or failed
- `&bytes_read1` gets filled with how much data was actually read

---

```cpp
if (err1 != ESP_OK || err2 != ESP_OK) {
  Serial.printf("Read error: mic1=%d, mic2=%d\n", err1, err2);
  return;
}
```
**What this does:** Checks if reading audio data failed. If either microphone had problems, print an error message and skip the rest of this loop cycle.

---

```cpp
if (bytes_read1 == 0 || bytes_read2 == 0) {
  Serial.println("No data read from microphones");
  return;
}
```
**What this does:** Double-checks that we actually got some audio data. If not, print a message and try again.

---

```cpp
int sample_count1 = bytes_read1 / sizeof(int16_t);
int sample_count2 = bytes_read2 / sizeof(int16_t);
int sample_count = min(sample_count1, sample_count2);
```
**What this does:** 
- Converts bytes to number of samples (divide by 2 because each sample is 2 bytes)
- Uses the smaller number of samples to ensure we don't process bad data

## Audio Mixing Section

```cpp
for(int i = 0; i < sample_count; i++) {
```
**What this does:** Starts a loop that processes each audio sample one by one.

---

```cpp
int32_t sample1 = mic1_samples[i];
int32_t sample2 = mic2_samples[i];
```
**What this does:** Gets one sample from each microphone and stores them in larger variables (32-bit instead of 16-bit) to prevent math errors.

---

```cpp
int32_t mixed = (sample1 + sample2) / 2;
```
**What this does:** Mixes the two audio samples by averaging them. If mic1 has volume 100 and mic2 has volume 200, the result is (100+200)/2 = 150.

---

```cpp
if (mixed > 32767) mixed = 32767;
if (mixed < -32768) mixed = -32768;
```
**What this does:** Prevents audio distortion by limiting the volume. 16-bit audio can only handle values from -32,768 to +32,767. If our mixed audio is louder, clip it to the maximum.

---

```cpp
mic1_samples[i] = (int16_t)mixed;
```
**What this does:** Stores the mixed audio sample back into the first microphone's array, converting back to 16-bit format.

## Audio Output Section

```cpp
size_t bytes_to_write = sample_count * sizeof(int16_t);
size_t bytes_written;
```
**What this does:** Calculates how much data to send to the speakers and creates a variable to track how much was actually sent.

---

```cpp
esp_err_t write_err = i2s_write(I2S_NUM_0, mic1_samples, bytes_to_write, &bytes_written, pdMS_TO_TICKS(10));
```
**What this does:** Sends the mixed audio to the speakers/amplifier through I2S bus 0, with a 10ms timeout.

---

```cpp
if (write_err != ESP_OK) {
  Serial.printf("Write error: %d\n", write_err);
}
```
**What this does:** Checks if sending audio to speakers failed, and prints an error message if so.

## Debug Information

```cpp
static int debug_counter = 0;
if (++debug_counter % 1000 == 0) {
  Serial.printf("Samples: mic1=%d, mic2=%d, mixed=%d, written=%d\n", 
                sample_count1, sample_count2, sample_count, bytes_written/sizeof(int16_t));
}
```
**What this does:** 
- **static int debug_counter = 0:** Creates a counter that remembers its value between loop cycles
- **++debug_counter % 1000 == 0:** Every 1000 loops, print debug information
- Shows how many samples were processed from each microphone and how many were sent to speakers

## Summary

This code continuously:
1. Reads audio from two microphones
2. Mixes them together by averaging
3. Sends the mixed audio to speakers
4. Handles errors gracefully
5. Provides debug information

The entire process repeats thousands of times per second to create real-time audio mixing!
 */
