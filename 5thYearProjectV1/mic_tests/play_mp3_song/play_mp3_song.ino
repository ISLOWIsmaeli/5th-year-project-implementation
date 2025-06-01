#include "Arduino.h"
#include "Audio.h"
#include "SD.h"
#include "FS.h"

//
// microSD card to ESP32 connections
//

#define CARD_MOSI 23
#define CARD_MISO 19
#define CARD_SCK  18
#define CARD_CS    5

//
// Define MAX98357A amplifier connections
//

#define I2S_BCLK 4
#define I2S_LRC  21
#define I2S_DOUT 22

Audio audio;                                      // Audio object

void setup()
{
  pinMode(CARD_CS, OUTPUT);                     // CS is output   
  digitalWrite(CARD_CS, HIGH);                    // CS HIGH
  SPI.begin(CARD_SCK,CARD_MISO,CARD_MOSI);      // Init SPI bus

  Serial.begin(115200);                         // Serial Monitor
  if(!SD.begin(CARD_CS))                        // Start SD card
  {
    Serial.println("Error in microSD card");    // card error
    while(true);
  }
  Serial.println("microSD card accessed correctly");
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT); // Setup I2S
  audio.setVolume(10);                          // Set volume
  audio.connecttoFS(SD,"A Mighty Fortress  SDA Hymn #506  Breath of Praise.mp3");       // Open file
}
void loop()
{
audio.loop();
}
