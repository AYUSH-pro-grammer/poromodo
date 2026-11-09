#include <Arduino.h>

#define P_BAT_SENSE D1
#define P_BUZZER    D2
#define P_MPU_INT   D3
#define P_SDA       D4
#define P_SCL       D5
#define P_BTN1      D6
#define P_BTN2      D7
#define P_LED       D10

bool lastBtn1 = false;
bool lastBtn2 = false;
bool ledState = false;
uint32_t lastBlink = 0;

void printPinMap() {
  Serial.println();
  Serial.println("=====================================");
  Serial.println("  Pomodoro timer - hello pins");
  Serial.println("=====================================");
  Serial.printf("Chip: %s, %d MHz, free heap %u bytes\n",
                ESP.getChipModel(), (int)ESP.getCpuFreqMHz(), (unsigned)ESP.getFreeHeap());
  Serial.println("Pin map:");
  Serial.println("  D1  BAT_SENSE (battery divider)");
  Serial.println("  D2  BUZZER");
  Serial.println("  D3  MPU_INT");
  Serial.println("  D4  SDA");
  Serial.println("  D5  SCL");
  Serial.println("  D6  BTN1");
  Serial.println("  D7  BTN2");
  Serial.println("  D10 LED1");
  Serial.println("Press the buttons - you should see messages below.");
  Serial.println();
}

void setup() {
  Serial.begin(115200);

  uint32_t start = millis();
  while (!Serial && millis() - start < 3000) {
    delay(10);
  }

  pinMode(P_LED, OUTPUT);
  pinMode(P_BTN1, INPUT_PULLUP);
  pinMode(P_BTN2, INPUT_PULLUP);

  printPinMap();
}

void loop() {

  if (millis() - lastBlink >= 500) {
    lastBlink = millis();
    ledState = !ledState;
    digitalWrite(P_LED, ledState ? HIGH : LOW);
  }

  bool b1 = (digitalRead(P_BTN1) == LOW);
  bool b2 = (digitalRead(P_BTN2) == LOW);

  if (b1 != lastBtn1) {
    lastBtn1 = b1;
    Serial.println(b1 ? "BTN1 pressed" : "BTN1 released");
  }
  if (b2 != lastBtn2) {
    lastBtn2 = b2;
    Serial.println(b2 ? "BTN2 pressed" : "BTN2 released");
  }
}
