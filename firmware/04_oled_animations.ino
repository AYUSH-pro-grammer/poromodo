#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <U8g2lib.h>

#define P_SDA  D4
#define P_SCL  D5
#define P_BTN1 D6
#define P_BTN2 D7

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

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

const uint8_t NUM_ANIMS = 6;
const char* const ANIM_NAMES[NUM_ANIMS] = {"Ball", "Spinner", "Ring", "Marquee", "Tomato", "Sine"};
uint8_t anim = 0;

const float SPEEDS[3] = {0.5f, 1.0f, 2.0f};
uint8_t speedIdx = 1;

float animTime = 0;
uint32_t lastFrame = 0;

float bx = 20, by = 30, bvx = 70, bvy = 45;
void drawBall(float dt) {
  int W = u8g2.getDisplayWidth();
  int H = u8g2.getDisplayHeight();
  const int R = 5;
  bx += bvx * dt;
  by += bvy * dt;
  if (bx < R)         { bx = R;         bvx = -bvx; }
  if (bx > W - R - 1) { bx = W - R - 1; bvx = -bvx; }
  if (by < R)         { by = R;         bvy = -bvy; }
  if (by > H - R - 1) { by = H - R - 1; bvy = -bvy; }
  u8g2.drawFrame(0, 0, W, H);
  u8g2.drawDisc((int)bx, (int)by, R);
}

void drawSpinner() {
  int cx = u8g2.getDisplayWidth() / 2;
  int cy = u8g2.getDisplayHeight() / 2;
  const int DOTS = 10;
  int lead = (int)(animTime * 8) % DOTS;
  for (int i = 0; i < DOTS; i++) {
    float ang = i * 2 * PI / DOTS - PI / 2;
    int x = cx + (int)(22 * cosf(ang));
    int y = cy + (int)(22 * sinf(ang));
    int age = (lead - i + DOTS) % DOTS;
    int r = 4 - age / 3;
    if (r < 1) r = 1;
    u8g2.drawDisc(x, y, r);
  }
}

void drawRing() {
  int cx = u8g2.getDisplayWidth() / 2;
  int cy = u8g2.getDisplayHeight() / 2;
  const float PERIOD = 10.0f;
  float p = fmodf(animTime, PERIOD) / PERIOD;
  int ticks = (int)(p * 60);
  for (int i = 0; i < 60; i++) {
    float ang = i * 2 * PI / 60 - PI / 2;
    int r1 = (i % 5 == 0) ? 22 : 25;
    int x1 = cx + (int)(r1 * cosf(ang));
    int y1 = cy + (int)(r1 * sinf(ang));
    int x2 = cx + (int)(29 * cosf(ang));
    int y2 = cy + (int)(29 * sinf(ang));
    if (i < ticks) u8g2.drawLine(x1, y1, x2, y2);
    else u8g2.drawPixel(x2, y2);
  }
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", (int)(PERIOD - p * PERIOD) + 1);
  u8g2.setFont(u8g2_font_helvB14_tn);
  int w = u8g2.getStrWidth(buf);
  u8g2.drawStr(cx - w / 2, cy + 7, buf);
}

void drawMarquee() {
  int W = u8g2.getDisplayWidth();
  int H = u8g2.getDisplayHeight();
  const char* text = "POMODORO TIMER - focus for 25 minutes, then rest!   ";
  u8g2.setFont(u8g2_font_helvB14_tr);
  int w = u8g2.getStrWidth(text);
  int x = W - (int)(animTime * 60) % (w + W);
  u8g2.drawStr(x, H / 2 + 6, text);
}

void drawTomato() {
  int cx = u8g2.getDisplayWidth() / 2;
  int cy = u8g2.getDisplayHeight() / 2 + 3;
  int r = 17 + (int)(3 * sinf(animTime * 3));
  u8g2.drawDisc(cx, cy, r);

  u8g2.setDrawColor(0);
  u8g2.drawTriangle(cx, cy - r + 4, cx - 8, cy - r + 8, cx - 2, cy - r + 10);
  u8g2.drawTriangle(cx, cy - r + 4, cx + 8, cy - r + 8, cx + 2, cy - r + 10);
  u8g2.setDrawColor(1);
  u8g2.drawBox(cx - 1, cy - r - 5, 3, 6);
}

void drawSine() {
  int W = u8g2.getDisplayWidth();
  int H = u8g2.getDisplayHeight();
  int prevY = H / 2;
  for (int x = 0; x < W; x++) {
    int y = H / 2 + (int)((H / 3) * sinf((x + animTime * 40) * 0.12f));
    if (x > 0) u8g2.drawLine(x - 1, prevY, x, y);
    prevY = y;
  }
}

void setup() {
  Wire.begin(P_SDA, P_SCL);
  u8g2.begin();
  u8g2.setBusClock(400000);
  btn1.begin(P_BTN1);
  btn2.begin(P_BTN2);
  lastFrame = millis();
}

void loop() {
  btn1.update();
  btn2.update();
  if (btn1.pressedEvt) anim = (anim + 1) % NUM_ANIMS;
  if (btn2.pressedEvt) speedIdx = (speedIdx + 1) % 3;

  uint32_t now = millis();
  float dt = (now - lastFrame) / 1000.0f * SPEEDS[speedIdx];
  lastFrame = now;
  animTime += dt;

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tf);
  switch (anim) {
    case 0: drawBall(dt);  break;
    case 1: drawSpinner(); break;
    case 2: drawRing();    break;
    case 3: drawMarquee(); break;
    case 4: drawTomato();  break;
    case 5: drawSine();    break;
  }

  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setDrawColor(1);
  u8g2.drawStr(2, 7, ANIM_NAMES[anim]);
  u8g2.sendBuffer();
  delay(15);
}
