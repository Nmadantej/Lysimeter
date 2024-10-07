#include <SPI.h>
#include "RF24.h"
#include <Wire.h>
#include <DS3231-RTC.h>
#include <LiquidCrystal_I2C.h>
#include <HX711_ADC.h>
#include <EEPROM.h>
#include <SD.h>

RF24 radio(9, 10); // CE, CSN
const byte address[6] = "Node1"; // Change this for each slave (Node1, Node2, ..., Node29)
DS3231 myRTC;
LiquidCrystal_I2C lcd(0x27, 16, 2);

const int HX711_dout = 3;
const int HX711_sck = 4;
const int sdCardPin = 10;
const int solenoidPin = 5; // Pin connected to the solenoid
int select = 8; // Button for tare and calibration
const int calVal_eepromAdress = 0;

HX711_ADC LoadCell(HX711_dout, HX711_sck);
File dataFile;

float calibrationValue;
const float lowerThreshold = 100.0; // Adjustable lower threshold
const float upperThreshold = 500.0;  // Adjustable upper threshold

void setup() {
  pinMode(solenoidPin, OUTPUT);
  digitalWrite(solenoidPin, LOW); // Ensure solenoid is closed initially

  pinMode(select, INPUT_PULLUP);
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Slave Starting");

  Serial.begin(9600);
  
  EEPROM.get(calVal_eepromAdress, calibrationValue);
  LoadCell.begin();
  LoadCell.start(2000, true);
  LoadCell.setCalFactor(calibrationValue);

  // Initialize the SD card
  if (!SD.begin(sdCardPin)) {
    Serial.println("SD card initialization failed!");
    while (1);
  }

  // Open a file for writing
  dataFile = SD.open("data.txt", FILE_WRITE);
  if (!dataFile) {
    Serial.println("Error opening data.txt");
  }

  // Begin radio communication
  radio.begin();
  radio.openReadingPipe(1, address); // Open reading pipe with unique address
  radio.startListening();
}

void loop() {
  if (radio.available()) {
    char text[32] = "";
    radio.read(&text, sizeof(text));

    // Simulate data collection
    float weight = LoadCell.getData();

    // Get current date and time from RTC
    bool century;
    byte month = myRTC.getMonth(century);
    byte date = myRTC.getDate();
    byte year = myRTC.getYear();
    bool h12, PM;
    byte hour = myRTC.getHour(h12, PM);
    byte minute = myRTC.getMinute();
    byte second = myRTC.getSecond();

    // Error detection: Check if weight is abnormal
    if (weight < 0 || weight > 1023) { // Adjust limits as necessary
      radio.stopListening();
      const char* errorMsg = "ERROR"; // Indicate an error
      radio.write(errorMsg, sizeof(errorMsg));
      radio.startListening();
    } else {
      // Send the sensor data back to master
      radio.stopListening();
      radio.write(&weight, sizeof(weight));
      radio.startListening();

      // Log data to SD card with date and time
      if (dataFile) {
        dataFile.print(year);
        dataFile.print("/");
        dataFile.print(month);
        dataFile.print("/");
        dataFile.print(date);
        dataFile.print(" ");
        dataFile.print(hour);
        dataFile.print(":");
        dataFile.print(minute);
        dataFile.print(":");
        dataFile.print(second);
        dataFile.print(" Weight: ");
        dataFile.println(weight);
        dataFile.flush(); // Ensure data is written to SD card
      } else {
        Serial.println("Error writing to SD card.");
      }

      // Control the solenoid based on weight
      if (weight < lowerThreshold) {
        digitalWrite(solenoidPin, HIGH); // Open solenoid
      } else if (weight >= upperThreshold) {
        digitalWrite(solenoidPin, LOW); // Close solenoid
      }
    }
  }

  // Recalibration logic
  if (digitalRead(select) == LOW) { // Check if the calibration button is pressed
    calib();
  }
}

void calib() {
  lcd.clear();
  lcd.print("Calibrating...");
  Serial.println("Starting calibration...");

  // Wait for the user to tare
  LoadCell.tareNoDelay();
  while (!LoadCell.getTareStatus()) {
    // Do nothing until tare is complete
  }

  // Wait for the user to put known weight
  lcd.clear();
  lcd.print("Place weight");
  float known_mass = 100; // Set the known mass for calibration
  Serial.println("Place the known weight...");

  LoadCell.refreshDataSet();
  float newCalibrationValue = LoadCell.getNewCalibration(known_mass);
  
  // Save new calibration value to EEPROM
  EEPROM.put(calVal_eepromAdress, newCalibrationValue);
  EEPROM.get(calVal_eepromAdress, newCalibrationValue);
  
  Serial.print("New calibration value saved: ");
  Serial.println(newCalibrationValue);
  
  lcd.clear();
  lcd.print("Calibrated!");
  delay(2000);
}
