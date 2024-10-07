#include <SD.h>
#include <Arduino.h>
#include <Wire.h>
#include <DS3231-RTC.h>
#include <LiquidCrystal_I2C.h>
#include <HX711_ADC.h>
#include <EEPROM.h>

DS3231 myRTC;
LiquidCrystal_I2C lcd(0x27, 16, 2);

const int HX711_dout = 3;
const int HX711_sck = 4;
const int solenoidPin = 2;  
const int sdCardPin = 10;
int select = 8;
int state = 7;
const int calVal_eepromAdress = 0;

HX711_ADC LoadCell(HX711_dout, HX711_sck);
File dataFile;

float calibrationValue;
unsigned long lastLCDUpdateTime = 0;
unsigned long lastSDWriteTime = 0;
const int LCDUpdateInterval = 1000;   
const int SDWriteInterval = 15000;    

float weightThresholdOpen = 700;   
float weightThresholdClose = 900;  

void setup() {
  pinMode(select, INPUT_PULLUP);
  pinMode(state, INPUT_PULLUP);
  pinMode(solenoidPin, OUTPUT);    
  digitalWrite(solenoidPin, LOW);  

  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Starting");
  myRTC.setClockMode(false);  
  myRTC.setYear(24);
  myRTC.setMonth(9);
  myRTC.setDate(30);
  myRTC.setHour(13);
  myRTC.setMinute(20);
  myRTC.setSecond(0);
  delay(500);
  lcd.clear();
  Serial.begin(9600);

  EEPROM.get(calVal_eepromAdress, calibrationValue);

  LoadCell.begin();
  unsigned long stabilizingtime = 2000;
  boolean _tare = true;
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag()) {
    while (1);
  } else {
    LoadCell.setCalFactor(calibrationValue);
  }

  pinMode(sdCardPin, OUTPUT);
  if (!SD.begin(sdCardPin)) {
    Serial.println("SD card initialization failed!");
    while (1);
  }
  Serial.println("Date    Time   Weight(gms)");

  if (digitalRead(state) == 0) {
    while (1) {
      Serial.println("Calibration mode");
      lcd.print("Calibration mode");
      calib();
    }
  }
}

void loop() {
  bool century;
  byte month = myRTC.getMonth(century);
  byte date = myRTC.getDate();
  byte year = myRTC.getYear();
  bool h12, PM;
  byte hour = myRTC.getHour(h12, PM);
  byte minute = myRTC.getMinute();
  byte second = myRTC.getSecond();

  static boolean newDataReady = 0;
  if (LoadCell.update()) newDataReady = true;

  if (newDataReady) {
    float weight = LoadCell.getData();

    if (millis() - lastLCDUpdateTime > LCDUpdateInterval) {
      lcd.setCursor(0, 0);
      lcd.print(date, DEC);
      lcd.print("/");
      lcd.print(month, DEC);
      lcd.print("/");
      lcd.print(year, DEC);
      lcd.print(" ");
      lcd.print(hour, DEC);
      lcd.print(":");
      lcd.print(minute, DEC);
      //lcd.print(":");
      //lcd.print(second, DEC);
      //lcd.print(" ");
      lcd.setCursor(0, 1);
      lcd.print("    W:");
      lcd.print(weight);
      lastLCDUpdateTime = millis();
    }

    // Log data to SD card
    if (millis() - lastSDWriteTime > SDWriteInterval) {
      Serial.print(date, DEC);
      Serial.print("/");
      Serial.print(month, DEC);
      Serial.print("/");
      Serial.print(year, DEC);
      Serial.print(" ");
      Serial.print(hour, DEC);
      Serial.print(":");
      Serial.print(minute, DEC);
      Serial.print(":");
      Serial.print(second, DEC);
      Serial.print(" ");
      Serial.print(weight);
      Serial.println(" g");

      dataFile = SD.open("data.txt", FILE_WRITE);
      if (dataFile) {
        dataFile.print(date, DEC);
        dataFile.print("/");
        dataFile.print(month, DEC);
        dataFile.print("/");
        dataFile.print(year, DEC);
        dataFile.print(" ");
        dataFile.print(hour, DEC);
        dataFile.print(":");
        dataFile.print(minute, DEC);
        dataFile.print(":");
        dataFile.print(second, DEC);
        dataFile.print(" ");
        dataFile.print(weight);
        dataFile.println(" g");
        dataFile.close();
      } else {
        Serial.println("Error writing to SD card.");
      }
      lastSDWriteTime = millis();  
    }

    
    if (weight > weightThresholdOpen) {
      digitalWrite(solenoidPin, HIGH);  
      lcd.setCursor(0, 1);
      lcd.print("V:O");
    } else if (weight < weightThresholdClose) {
      digitalWrite(solenoidPin, LOW);   
      lcd.setCursor(0, 1);
      lcd.print("V:C");
    }

    newDataReady = 0;
  }

  if (digitalRead(select) == 0) {
    LoadCell.tareNoDelay();
  }

  if (LoadCell.getTareStatus() == true) {
    Serial.println("Tare complete");
  }
}

void calib() {
  while (!LoadCell.update());
  boolean _resume = false;
  while (_resume == false) {
    LoadCell.update();
    if (digitalRead(select) == 0) {
      LoadCell.tareNoDelay();
    }

    if (LoadCell.getTareStatus() == true) {
      _resume = true;
    }
  }

  float known_mass = 0;
  _resume = false;
  while (_resume == false) {
    LoadCell.update();
    if (digitalRead(select) == 0) {
      known_mass = 100;
      _resume = true;
    }
  }

  LoadCell.refreshDataSet();
  float newCalibrationValue = LoadCell.getNewCalibration(known_mass);
  _resume = false;
  while (_resume == false) {
    if (digitalRead(select) == 0) {
      EEPROM.put(calVal_eepromAdress, newCalibrationValue);
      EEPROM.get(calVal_eepromAdress, newCalibrationValue);
      Serial.print("Value ");
      Serial.print(newCalibrationValue);
      Serial.print(" saved to EEPROM address: ");
      Serial.println(calVal_eepromAdress);
      _resume = true;
    }
  }

  Serial.println("End calibration");
  Serial.println("***");
  while (1);
}
