#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#define P_SDA       D4
#define P_SCL       D5
#define P_BAT_SENSE D1
#define P_BUZZER    D2
#define P_LED       D10
#define P_BTN1      D6

const float R_TOP = 100000.0f;
const float R_BOTTOM = 33000.0f;
const float CALIBRATION = 1.00f;

const float LOW_BATT_V = 6.6f;
const float EMPTY_V = 6.0f;
const float FULL_V = 8.4f;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

struct CurvePoint { float v; int pct; };
const CurvePoint CURVE[] = {
  {3.00f, 0}, {3.30f, 5}, {3.50f, 15}, {3.60f, 30}, {3.70f, 45},
  {3.80f, 60}, {3.90f, 72}, {4.00f, 84}, {4.10f, 94}, {4.20f, 100},
};
const int CURVE_N = sizeof(CURVE) / sizeof(CurvePoint);

int percentFromCell(float v) {
  if (v <= CURVE[0].v) return 0;
  if (v >= CURVE[CURVE_N - 1].v) return 100;
  for (int i = 1; i < CURVE_N; i++) {
    if (v <= CURVE[i].v) {
      float t = (v - CURVE[i - 1].v) / (CURVE[i].v - CURVE[i - 1].v);
      return CURVE[i - 1].pct + (int)(t * (CURVE[i].pct - CURVE[i - 1].pct));
    }
  }
  return 100;
}

float readBatteryVolts() {
  uint32_t sum = 0;
  const int N = 32;
  for (int i = 0; i < N; i++) {
    sum += analogReadMilliVolts(P_BAT_SENSE);
    delay(2);
  }
  float pinVolts = (sum / (float)N) / 1000.0f;
  return pinVolts * (R_TOP + R_BOTTOM) / R_BOTTOM * CALIBRATION;
}

void drawBatteryIcon(int x, int y, int pct) {
  u8g2.drawFrame(x, y, 40, 18);
  u8g2.drawBox(x + 40, y + 5, 3, 8);
  int fill = (36 * pct) / 100;
  u8g2.drawBox(x + 2, y + 2, fill, 14);
}

float volts = 0;
int pct = 0;
bool noBattery = false;
bool lowWarned = false;

void readAndDraw() {
  volts = readBatteryVolts();
  noBattery = volts < 2.0f;
  pct = percentFromCell(volts / 2.0f);

  Serial.printf("pin-side voltage -> battery: %.2f V  (%d %%)%s\n", volts, pct, noBattery ? "  [no battery]" : "");

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "Battery monitor (2S)");
  u8g2.drawHLine(0, 12, 128);

  if (noBattery) {
    u8g2.setFont(u8g2_font_helvB10_tf);
    u8g2.drawStr(10, 36, "No battery");
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(10, 52, "(USB power only)");
  } else {
    drawBatteryIcon(4, 20, pct);
    char buf[24];
    u8g2.setFont(u8g2_font_logisoso20_tn);
    snprintf(buf, sizeof(buf), "%d", pct);
    int numW = u8g2.getStrWidth(buf);
    u8g2.drawStr(54, 40, buf);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(54 + numW + 2, 40, "%");
    snprintf(buf, sizeof(buf), "%.2f V", volts);
    u8g2.drawStr(4, 56, buf);
    snprintf(buf, sizeof(buf), "%.2f V/cell", volts / 2.0f);
    u8g2.drawStr(54, 56, buf);
    if (volts < LOW_BATT_V) {
      u8g2.drawStr(4, 46, "LOW - charge me!");
    }
  }
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  uint32_t start = millis();
  while (!Serial && millis() - start < 2000) delay(10);

  analogSetPinAttenuation(P_BAT_SENSE, ADC_11db);
  pinMode(P_LED, OUTPUT);
  pinMode(P_BTN1, INPUT_PULLUP);
  ledcAttach(P_BUZZER, 2000, 8);
  ledcWriteTone(P_BUZZER, 0);

  Wire.begin(P_SDA, P_SCL);
  u8g2.begin();
  u8g2.setBusClock(400000);
}

uint32_t lastRead = 0;

void loop() {
  if (millis() - lastRead > 1000) {
    lastRead = millis();
    readAndDraw();

    if (!noBattery && volts < LOW_BATT_V) {
      digitalWrite(P_LED, HIGH);
      if (!lowWarned) {
        lowWarned = true;
        ledcWriteTone(P_BUZZER, 1500);
        delay(300);
        ledcWriteTone(P_BUZZER, 0);
      }
    } else {
      digitalWrite(P_LED, LOW);
      if (volts > LOW_BATT_V + 0.2f) lowWarned = false;
    }
  }
}
