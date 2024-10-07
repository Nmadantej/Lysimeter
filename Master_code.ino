#include <SPI.h>
#include "RF24.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

RF24 radio(9, 10); // CE, CSN
LiquidCrystal_I2C lcd(0x27, 16, 2);

const int numSlaves = 29;
int slaveAddresses[numSlaves] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 
                                  10, 11, 12, 13, 14, 15, 16, 17, 
                                  18, 19, 20, 21, 22, 23, 24, 25, 
                                  26, 27, 28}; // Addresses for slaves

float weights[numSlaves];
bool alarms[numSlaves];

void setup() {
  lcd.init();
  lcd.backlight();
  Serial.begin(9600);
  radio.begin();
  radio.openReadingPipe(1, 0xF0F0F0F0E1LL); // Address for master
  radio.startListening();
  lcd.print("Master Ready");
}

void loop() {
  for (int i = 0; i < numSlaves; i++) {
    if (radio.available()) {
      char text[32] = "";
      radio.read(&text, sizeof(text));

      // Read weight from slave
      float weight;
      radio.read(&weight, sizeof(weight));

      // Check for errors
      if (strcmp(text, "ERROR") == 0) {
        alarms[i] = true;
      } else {
        alarms[i] = false; // Reset alarm if no error
        weights[i] = weight; // Store the weight
      }

      // Display the result on LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Slave:");
      lcd.print(i);
      lcd.setCursor(0, 1);
      lcd.print("Weight: ");
      lcd.print(weights[i]);
      
      // Indicate alarm if there is an issue
      if (alarms[i]) {
        lcd.setCursor(0, 1);
        lcd.print("ALARM SLAVE ");
        lcd.print(i);
      }
    }

    // Add delay for reading slaves
    delay(100);
  }
}
