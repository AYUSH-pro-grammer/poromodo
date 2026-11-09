#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#define P_SDA    D4
#define P_SCL    D5
#define P_BUZZER D2
#define P_BTN1   D6
#define P_BTN2   D7
#define P_LED    D10

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

#define NOTE_C5 523
#define NOTE_D5 587
#define NOTE_E5 659
#define NOTE_F5 698
#define NOTE_G5 784
#define NOTE_A5 880
#define NOTE_B5 988
#define NOTE_C6 1047
#define NOTE_E6 1319
#define NOTE_G6 1568
#define NOTE_C7 2093
#define REST    0

struct Note { uint16_t freq; uint16_t ms; };

const Note MEL_START[]  = {{NOTE_C6, 120}, {NOTE_E6, 120}, {NOTE_G6, 220}};
const Note MEL_ALARM[]  = {{NOTE_C7, 150}, {REST, 80}, {NOTE_C7, 150}, {REST, 80}, {NOTE_C7, 150}, {REST, 400},
                           {NOTE_C7, 150}, {REST, 80}, {NOTE_C7, 150}, {REST, 80}, {NOTE_C7, 150}};
const Note MEL_BREAK[]  = {{NOTE_G6, 140}, {NOTE_E6, 140}, {NOTE_C6, 260}};
const Note MEL_HAPPY[]  = {{NOTE_C5, 150}, {NOTE_E5, 150}, {NOTE_G5, 150}, {NOTE_C6, 300}, {REST, 100},
                           {NOTE_G5, 150}, {NOTE_C6, 400}};

struct Melody { const char* name; const Note* notes; uint8_t len; };
const Melody MELODIES[] = {
  {"Start chime", MEL_START, sizeof(MEL_START) / sizeof(Note)},
  {"Alarm",       MEL_ALARM, sizeof(MEL_ALARM) / sizeof(Note)},
  {"Break chime", MEL_BREAK, sizeof(MEL_BREAK) / sizeof(Note)},
  {"Happy tune",  MEL_HAPPY, sizeof(MEL_HAPPY) / sizeof(Note)},
};
const uint8_t NUM_MELODIES = sizeof(MELODIES) / sizeof(Melody);

const Note* playing = nullptr;
uint8_t playLen = 0, playIdx = 0;
uint32_t noteStart = 0;

void buzzerOff() { ledcWriteTone(P_BUZZER, 0); }

void startMelody(const Melody& m) {
  playing = m.notes;
  playLen = m.len;
  playIdx = 0;
  noteStart = millis();
  if (playing[0].freq) ledcWriteTone(P_BUZZER, playing[0].freq);
  else buzzerOff();
}

void stopMelody() {
  playing = nullptr;
  buzzerOff();
}

void updateMelody() {
  if (!playing) return;
  if (millis() - noteStart < playing[playIdx].ms) return;
  playIdx++;
  if (playIdx >= playLen) { stopMelody(); return; }
  noteStart = millis();
  if (playing[playIdx].freq) ledcWriteTone(P_BUZZER, playing[playIdx].freq);
  else buzzerOff();
}

const char* const LED_NAMES[] = {"off", "on", "slow blink", "fast blink", "heartbeat", "breathing"};
const uint8_t NUM_LED_PATTERNS = 6;
uint8_t ledPattern = 0;

void updateLed() {
  uint32_t t = millis();
  int level = 0;
  switch (ledPattern) {
    case 0: level = 0; break;
    case 1: level = 255; break;
    case 2: level = ((t / 500) % 2) ? 255 : 0; break;
    case 3: level = ((t / 120) % 2) ? 255 : 0; break;
    case 4: {
      uint32_t p = t % 1200;
      level = (p < 100 || (p > 220 && p < 320)) ? 255 : 0;
      break;
    }
    case 5: {
      float s = (sinf(t / 1000.0f * 2.0f * PI) + 1.0f) / 2.0f;
      level = (int)(s * s * 255);
      break;
    }
  }
  analogWrite(P_LED, level);
}

struct Btn {
  uint8_t pin;
  bool lastRaw = false, stable = false, shortEvt = false, longEvt = false, longFired = false;
  uint32_t changedAt = 0, pressedAt = 0;
  void begin(uint8_t p) {
    pin = p;
    pinMode(p, INPUT_PULLUP);
    stable = lastRaw = (digitalRead(p) == LOW);
  }
  void update() {
    shortEvt = longEvt = false;
    bool raw = (digitalRead(pin) == LOW);
    uint32_t now = millis();
    if (raw != lastRaw) { lastRaw = raw; changedAt = now; }
    if (now - changedAt > 25 && raw != stable) {
      stable = raw;
      if (stable) { pressedAt = now; longFired = false; }
      else if (!longFired) shortEvt = true;
    }
    if (stable && !longFired && now - pressedAt > 800) { longFired = true; longEvt = true; }
  }
};
Btn btn1, btn2;

uint8_t melodyIdx = NUM_MELODIES - 1;

void draw() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "Buzzer / buttons / LED");
  u8g2.drawHLine(0, 12, 128);

  char buf[32];
  snprintf(buf, sizeof(buf), "Melody: %s", MELODIES[melodyIdx].name);
  u8g2.drawStr(0, 26, buf);
  u8g2.drawStr(0, 38, playing ? "Playing..." : "Silent");
  snprintf(buf, sizeof(buf), "LED: %s", LED_NAMES[ledPattern]);
  u8g2.drawStr(0, 50, buf);

  u8g2.drawStr(0, 62, "BTN1");
  if (btn1.stable) u8g2.drawBox(30, 54, 8, 8); else u8g2.drawFrame(30, 54, 8, 8);
  u8g2.drawStr(50, 62, "BTN2");
  if (btn2.stable) u8g2.drawBox(80, 54, 8, 8); else u8g2.drawFrame(80, 54, 8, 8);
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  uint32_t start = millis();
  while (!Serial && millis() - start < 2000) delay(10);

  btn1.begin(P_BTN1);
  btn2.begin(P_BTN2);
  pinMode(P_LED, OUTPUT);
  ledcAttach(P_BUZZER, 2000, 8);
  buzzerOff();

  Wire.begin(P_SDA, P_SCL);
  u8g2.begin();
  u8g2.setBusClock(400000);
  Serial.println("Press BTN1 for sound, BTN2 for LED patterns");
}

uint32_t lastDraw = 0;

void loop() {
  btn1.update();
  btn2.update();

  if (btn1.shortEvt) {
    melodyIdx = (melodyIdx + 1) % NUM_MELODIES;
    startMelody(MELODIES[melodyIdx]);
    Serial.printf("Playing: %s\n", MELODIES[melodyIdx].name);
  }
  if (btn1.longEvt) {
    stopMelody();
    Serial.println("Sound stopped");
  }
  if (btn2.shortEvt) {
    ledPattern = (ledPattern + 1) % NUM_LED_PATTERNS;
    Serial.printf("LED pattern: %s\n", LED_NAMES[ledPattern]);
  }
  if (btn2.longEvt) {
    ledPattern = 0;
    Serial.println("LED off");
  }

  updateMelody();
  updateLed();

  if (millis() - lastDraw > 50) {
    lastDraw = millis();
    draw();
  }
}
