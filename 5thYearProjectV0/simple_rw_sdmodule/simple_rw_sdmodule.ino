#include <SPI.h>
#include <SD.h>
// Define the chip select pin
const int chipSelect = 5;

void setup() {
  Serial.begin(115200);
  
  // Initialize SD card
  if (!SD.begin(chipSelect)) {
    Serial.println("Card initialization failed!");
    return;
  }
  Serial.println("Card initialized.");
  
  // Example array to store
  float sensorData[] = {23.5, 24.1, 22.8, 25.3, 21.9};
  int arraySize = sizeof(sensorData)/sizeof(sensorData[0]);
  
  // Write array to file
  writeArrayToFile("/data.txt", sensorData, arraySize);
  
  // Read and print file contents
  readFile("/data.txt");
}

void loop() {
  // Nothing to do here
}

void writeArrayToFile(const char* filename, float data[], int size) {
  // Open file for writing (creates if doesn't exist)
  File dataFile = SD.open(filename, FILE_WRITE);
  
  if (dataFile) {
    for (int i = 0; i < size; i++) {
      dataFile.print(data[i]);
      if (i < size-1) dataFile.print(","); // Add comma separator
    }
    dataFile.println(); // Add newline at end
    dataFile.close();
    Serial.println("Data written to file.");
  } else {
    Serial.println("Error opening file for writing!");
  }
}

void readFile(const char* filename) {
  // Open file for reading
  File dataFile = SD.open(filename);
  
  if (dataFile) {
    Serial.println("File contents:");
    while (dataFile.available()) {
      Serial.write(dataFile.read());
    }
    dataFile.close();
  } else {
    Serial.println("Error opening file for reading!");
  }
}
