#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#define P_SDA  D4
#define P_SCL  D5
#define P_BTN1 D6
#define P_BTN2 D7

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0,  U8X8_PIN_NONE);

struct Btn {
  uint8_t pin;
  bool lastRaw = false, stable = false, pressedEvt = false;
  uint32_t changedAt = 0;
  void begin(uint8_t p) {
    pin = p;
    pinMode(p, INPUT_PULLUP);
    stable = lastRaw = (digitalRead(p) == LOW);
  }
  void update() {
    pressedEvt = false;
    bool raw = (digitalRead(pin) == LOW);
    uint32_t now = millis();
    if (raw != lastRaw) { lastRaw = raw; changedAt = now; }
    if (now - changedAt > 25 && raw != stable) {
      stable = raw;
      if (stable) pressedEvt = true;
    }
  }
};

Btn btn1, btn2;

const uint8_t NUM_PAGES = 5;
uint8_t page = 0;

const uint8_t CONTRAST[] = {8, 60, 130, 255};
uint8_t contrastIdx = 3;

const u8g2_cb_t* const ROTATIONS[4] = {U8G2_R0, U8G2_R1, U8G2_R2, U8G2_R3};
const char* const ROT_NAMES[4] = {"0 deg", "90 deg", "180 deg", "270 deg"};
uint8_t rotIdx = 0;
uint32_t rotChangedAt = 0;

void drawTitle(const char* title) {
  int W = u8g2.getDisplayWidth();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, W, 11);
  u8g2.setDrawColor(0);
  u8g2.drawStr(2, 9, title);
  u8g2.setDrawColor(1);
}

void pageText() {
  drawTitle("1/5 Text & fonts");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 24, "Small font 6x10");
  u8g2.setFont(u8g2_font_helvB10_tf);
  u8g2.drawStr(0, 40, "Bold Helvetica");
  u8g2.setFont(u8g2_font_ncenB14_tr);
  u8g2.drawStr(0, 62, "Pomodoro!");
}

void pageShapes() {
  int W = u8g2.getDisplayWidth();
  int H = u8g2.getDisplayHeight();
  drawTitle("2/5 Shapes");
  u8g2.drawFrame(4, 16, 30, 20);
  u8g2.drawBox(40, 16, 30, 20);
  u8g2.drawRFrame(76, 16, 30, 20, 5);
  u8g2.drawCircle(18, 52, 9);
  u8g2.drawDisc(54, 52, 9);
  u8g2.drawTriangle(78, 62, 92, 42, 106, 62);
  u8g2.drawLine(0, 12, W - 1, H - 1);
}

void pageBars() {
  int W = u8g2.getDisplayWidth();
  drawTitle("3/5 Progress bars");

  uint32_t t = millis() % 4000;
  int percent = (t < 2000) ? (t * 100 / 2000) : ((4000 - t) * 100 / 2000);

  u8g2.setFont(u8g2_font_6x10_tf);
  char buf[16];
  snprintf(buf, sizeof(buf), "%d %%", percent);
  u8g2.drawStr(0, 26, buf);

  int barW = W - 4;
  u8g2.drawFrame(2, 32, barW, 12);
  u8g2.drawBox(4, 34, (barW - 4) * percent / 100, 8);

  int blocks = percent / 10;
  for (int i = 0; i < 10; i++) {
    int x = 2 + i * ((W - 4) / 10);
    if (i < blocks) u8g2.drawBox(x, 50, (W - 4) / 10 - 2, 10);
    else u8g2.drawFrame(x, 50, (W - 4) / 10 - 2, 10);
  }
}

void pageRotation() {

  if (millis() - rotChangedAt > 1500) {
    rotChangedAt = millis();
    rotIdx = (rotIdx + 1) % 4;
    u8g2.setDisplayRotation(ROTATIONS[rotIdx]);
  }
  int W = u8g2.getDisplayWidth();
  int H = u8g2.getDisplayHeight();
  drawTitle("4/5 Rotation");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(2, 26, ROT_NAMES[rotIdx]);
  char buf[20];
  snprintf(buf, sizeof(buf), "%d x %d px", W, H);
  u8g2.drawStr(2, 38, buf);

  int cx = W / 2;
  u8g2.drawTriangle(cx, H - 24, cx - 8, H - 12, cx + 8, H - 12);
  u8g2.drawBox(cx - 3, H - 12, 6, 10);
}

void pageClock() {
  int W = u8g2.getDisplayWidth();
  drawTitle("5/5 Uptime clock");
  uint32_t s = millis() / 1000;
  char buf[12];
  snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)((s / 60) % 100), (unsigned)(s % 60));
  u8g2.setFont(u8g2_font_logisoso28_tn);
  int w = u8g2.getStrWidth(buf);
  u8g2.drawStr((W - w) / 2, 56, buf);
}

void setup() {
  Wire.begin(P_SDA, P_SCL);
  u8g2.begin();
  u8g2.setBusClock(400000);
  u8g2.setContrast(CONTRAST[contrastIdx]);
  btn1.begin(P_BTN1);
  btn2.begin(P_BTN2);
}

void loop() {
  btn1.update();
  btn2.update();

  if (btn1.pressedEvt) {
    page = (page + 1) % NUM_PAGES;
    rotIdx = 0;
    u8g2.setDisplayRotation(U8G2_R0);
    rotChangedAt = millis();
  }
  if (btn2.pressedEvt) {
    contrastIdx = (contrastIdx + 1) % 4;
    u8g2.setContrast(CONTRAST[contrastIdx]);
  }

  u8g2.clearBuffer();
  switch (page) {
    case 0: pageText();     break;
    case 1: pageShapes();   break;
    case 2: pageBars();     break;
    case 3: pageRotation(); break;
    case 4: pageClock();    break;
  }
  u8g2.sendBuffer();
  delay(20);
}
