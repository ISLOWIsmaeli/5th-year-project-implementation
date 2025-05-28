#include "SD.h"
#define SD_CS 5  
void setup() 
{  
   Serial.begin(9600);
   Serial.println("");                        // New line
   if(!SD.begin(SD_CS))                       // Initialize library
   {
      Serial.println("Initialization failed");
      while(1);
   }
   else
      Serial.println("Initialization done");
 }
 //
 // This is the main program. File TEST.TXT is opened in directory
 // NUM of the SD card and its contents are displayed on the PC screen
 //
 void loop()
 {
    File MyFile;
    
    if(SD.cardType() == CARD_NONE)
    {
        Serial.println("SD card is not found");
        while(1);
    }
    MyFile = SD.open("/NUM/TEST.TXT", FILE_READ);     // Open file TEST.TXT
    if(MyFile)                                        // If file opened
    {
        Serial.println("File opened");                // Display message
        while(MyFile.available())                     // While there is data
        {
            Serial.write(MyFile.read());              // Write file contents
        } 
        MyFile.close();                               // Close the file
    }
    else
        Serial.println("Error in opening file");      // Display message
    while(1);                                         // Stop
 }   
