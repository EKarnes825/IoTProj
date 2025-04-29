#include <Wire.h>
#include <Adafruit_CCS811.h>

Adafruit_CCS811 ccs;

void setup() {

  Serial.begin(115200);

  // Checks I2C address for sensor
  Wire.begin();
  if (!ccs.begin(0x5A)) { 

    Serial.println("CCS811 not detected. Check wiring!");
    while (1);
  }

  // Sets measurement mode to every 1 second
  ccs.setDriveMode(CCS811_DRIVE_MODE_1SEC);

  // Sensor initialization
  while (!ccs.available());
}

void loop() {
  digitalWrite(2, HIGH);
  if (ccs.available()) {

    if (!ccs.readData()) {

      Serial.print("eCO2: ");
      Serial.print(ccs.geteCO2());
      Serial.print(" ppm, TVOC: ");
      Serial.print(ccs.getTVOC());
      Serial.println(" ppb");
    } else {

      Serial.println("Error reading data!");
    }
  }
  delay(1000); // In ms (has to match drive mode)
}