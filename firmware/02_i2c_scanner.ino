#include <Arduino.h>
#include <Wire.h>

#define P_SDA  D4
#define P_SCL  D5
#define P_LED  D10

const char* describe(uint8_t addr) {
  switch (addr) {
    case 0x3C: return "OLED display (SSD1306)";
    case 0x3D: return "OLED display (alternate address)";
    case 0x68: return "MPU6050 (AD0 low) - motion sensor";
    case 0x69: return "MPU6050 (AD0 high)";
    default:   return "unknown device";
  }
}

void scanBus() {
  Serial.println("Scanning I2C bus...");
  uint8_t found = 0;

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf("  found device at 0x%02X  -> %s\n", addr, describe(addr));
      found++;
    }
  }

  if (found == 0) {
    Serial.println("  no devices found! Check wiring and sockets.");
  } else {
    Serial.printf("Done: %u device(s) found.\n", found);
  }

  Wire.beginTransmission(0x68);
  Wire.write(0x75);
  if (Wire.endTransmission(false) == 0) {
    Wire.requestFrom((int)0x68, (int)1);
    if (Wire.available()) {
      uint8_t who = Wire.read();
      Serial.printf("  MPU WHO_AM_I = 0x%02X ", who);
      if (who == 0x68) Serial.println("(genuine MPU6050)");
      else Serial.println("(clone or a different chip - often still works)");
    }
  }
  Serial.println();

  for (uint8_t i = 0; i < found; i++) {
    digitalWrite(P_LED, HIGH);
    delay(120);
    digitalWrite(P_LED, LOW);
    delay(180);
  }
}

void setup() {
  Serial.begin(115200);
  uint32_t start = millis();
  while (!Serial && millis() - start < 3000) {
    delay(10);
  }
  pinMode(P_LED, OUTPUT);
  Wire.begin(P_SDA, P_SCL);
  Wire.setClock(400000);
  Serial.println("\n=== I2C scanner ===");
}

void loop() {
  scanBus();
  delay(3000);
}
