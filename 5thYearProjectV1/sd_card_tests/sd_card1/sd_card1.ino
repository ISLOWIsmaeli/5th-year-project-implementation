#include "SD.h"
#define SD_CS 5                             // SD card CS pin
void setup() 
{  
   Serial.begin(9600);
   Serial.println("");                      // New line
   if(!SD.begin(SD_CS))                     // Initialize SD card
   {
      Serial.println("Initialization failed");
      while(1);
   }
   else{
    Serial.println("Initialization done");
    }
 }
 //
 // This is the main program. A file called TEST.TXT is created in
 // directory NUM on the SD card. The program stores 10 integer random
 // integer numbers in this file on the SD card
 //
 void loop()
 {
    File MyFile;
    unsigned int RandomNumber;
    char k;
    
    if(SD.cardType() == CARD_NONE){
        Serial.println("SD card is not found");
        while(1);
    }
    SD.mkdir("/NUM");                                 // Create directory NUM
    MyFile = SD.open("/NUM/TEST.TXT", FILE_WRITE);    // Open file TEST.TXT
    if(MyFile)                                        // If file opened
    {
        Serial.println("Writing to SD card");         // Write heading
        MyFile.println("Random Numbers");             // Write heading
        MyFile.println("==============");             // Write heading
    
        for(k = 0; k < 10; k++)                       // Do 10 times
        {
            RandomNumber = random(0, 255);            // Generate number
            MyFile.println(RandomNumber);             // Save the number
            delay(1000);                              // Wait one second
        }
      
        MyFile.close();                               // Close the file
        Serial.println("Data written");               // Display message
    }
    else
        Serial.println("Error in opening file");      // Display message
    while(1);                                         // Stop
 }
