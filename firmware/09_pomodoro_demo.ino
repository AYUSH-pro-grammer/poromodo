#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <math.h>
#include <Preferences.h>

#define SECONDS_PER_MINUTE 3
const uint8_t FOCUS_MIN = 25;
const uint8_t SHORT_BREAK_MIN = 5;
const uint8_t LONG_BREAK_MIN = 15;
const uint8_t SESSIONS_BEFORE_LONG = 4;
#define FLIP_TO_FOCUS 1

#define P_BAT_SENSE D1
#define P_BUZZER    D2
#define P_SDA       D4
#define P_SCL       D5
#define P_BTN1      D6
#define P_BTN2      D7
#define P_LED       D10

#define MPU_ADDR 0x68

enum OriState : uint8_t {
  ORI_GRAV_DOWN = 0,
  ORI_GRAV_RIGHT,
  ORI_GRAV_UP,
  ORI_GRAV_LEFT
};

enum FaceState : uint8_t {
  FACE_NONE = 0,
  FACE_UP,
  FACE_DOWN
};

struct AxisCfg {
  int8_t axis = -1;
  int8_t sign = 1;
};

class Orientation {
 public:
  AxisCfg up, right, face;
  float a[3] = {0, 0, 1};
  OriState ori = ORI_GRAV_DOWN;
  FaceState faceState = FACE_NONE;

  bool begin() {
    Wire.beginTransmission(MPU_ADDR);
    if (Wire.endTransmission() != 0) return false;
    writeReg(0x6B, 0x80);
    delay(100);
    writeReg(0x6B, 0x01);
    writeReg(0x1A, 0x03);
    writeReg(0x19, 9);
    writeReg(0x1B, 0x00);
    writeReg(0x1C, 0x00);
    delay(50);
    return true;
  }

  bool readRaw(float out[3]) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)MPU_ADDR, (int)6) != 6) return false;
    for (int i = 0; i < 3; i++) {
      int16_t hi = Wire.read();
      int16_t lo = Wire.read();
      out[i] = (int16_t)((hi << 8) | lo) / 16384.0f;
    }
    return true;
  }

  bool calibrated() const { return up.axis >= 0 && right.axis >= 0 && face.axis >= 0; }

  bool load() {
    Preferences p;
    p.begin("orient", true);
    up.axis = p.getChar("ua", -1);    up.sign = p.getChar("us", 1);
    right.axis = p.getChar("ra", -1); right.sign = p.getChar("rs", 1);
    face.axis = p.getChar("fa", -1);  face.sign = p.getChar("fs", 1);
    p.end();
    return calibrated();
  }

  void save() {
    Preferences p;
    p.begin("orient", false);
    p.putChar("ua", up.axis);    p.putChar("us", up.sign);
    p.putChar("ra", right.axis); p.putChar("rs", right.sign);
    p.putChar("fa", face.axis);  p.putChar("fs", face.sign);
    p.end();
  }

  void clear() {
    Preferences p;
    p.begin("orient", false);
    p.clear();
    p.end();
    up = right = face = AxisCfg();
  }

  bool captureAxis(AxisCfg& out, int8_t skipA = -1, int8_t skipB = -1) {
    float sum[3] = {0, 0, 0};
    int n = 0;
    for (int i = 0; i < 30; i++) {
      float v[3];
      if (readRaw(v)) {
        sum[0] += v[0]; sum[1] += v[1]; sum[2] += v[2];
        n++;
      }
      delay(20);
    }
    if (n < 10) return false;
    int best = -1;
    float bestMag = 0;
    for (int i = 0; i < 3; i++) {
      if (i == skipA || i == skipB) continue;
      float m = fabsf(sum[i] / n);
      if (m > bestMag) { bestMag = m; best = i; }
    }
    if (best < 0 || bestMag < 0.7f) return false;
    out.axis = best;
    out.sign = (sum[best] > 0) ? 1 : -1;
    return true;
  }

  void update() {
    float v[3];
    if (!readRaw(v)) return;
    for (int i = 0; i < 3; i++) a[i] = a[i] * 0.8f + v[i] * 0.2f;
    if (!calibrated()) return;

    float u = up.sign * a[up.axis];
    float r = right.sign * a[right.axis];
    float f = face.sign * a[face.axis];

    uint32_t now = millis();

    FaceState candFace = FACE_NONE;
    if (f > 0.8f) candFace = FACE_UP;
    else if (f < -0.8f) candFace = FACE_DOWN;
    if (candFace != pendingFace) { pendingFace = candFace; faceSince = now; }
    if (pendingFace != faceState && now - faceSince > 500) faceState = pendingFace;

    if (candFace != FACE_NONE) return;
    bool haveCand = false;
    OriState cand = ori;
    if (fabsf(u) > fabsf(r) + 0.2f && fabsf(u) > 0.6f) {
      cand = (u > 0) ? ORI_GRAV_DOWN : ORI_GRAV_UP;
      haveCand = true;
    } else if (fabsf(r) > fabsf(u) + 0.2f && fabsf(r) > 0.6f) {
      cand = (r > 0) ? ORI_GRAV_LEFT : ORI_GRAV_RIGHT;
      haveCand = true;
    }
    if (!haveCand) { pendingOri = ori; oriSince = now; return; }
    if (cand != pendingOri) { pendingOri = cand; oriSince = now; }
    if (pendingOri != ori && now - oriSince > 350) ori = pendingOri;
  }

  const char* oriName() const {
    switch (ori) {
      case ORI_GRAV_DOWN:  return "upright";
      case ORI_GRAV_RIGHT: return "turned right";
      case ORI_GRAV_UP:    return "upside down";
      default:             return "turned left";
    }
  }

 private:
  FaceState pendingFace = FACE_NONE;
  OriState pendingOri = ORI_GRAV_DOWN;
  uint32_t faceSince = 0, oriSince = 0;

  void writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
  }
};

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Orientation orient;
bool haveMpu = false;
bool useOrientation = false;

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

struct Note { uint16_t freq; uint16_t ms; };
const Note MEL_START[] = {{1047, 100}, {1319, 160}};
const Note MEL_FOCUS_DONE[] = {{2093, 150}, {0, 80}, {2093, 150}, {0, 80}, {2093, 150}, {0, 350},
                               {2093, 150}, {0, 80}, {2093, 150}, {0, 80}, {2093, 150}};
const Note MEL_BREAK_DONE[] = {{1568, 140}, {1319, 140}, {1047, 260}};

const Note* playing = nullptr;
uint8_t playLen = 0, playIdx = 0;
uint32_t noteStart = 0;

void buzzerTone(uint16_t f) { ledcWriteTone(P_BUZZER, f); }

void playMelody(const Note* n, uint8_t len) {
  playing = n;
  playLen = len;
  playIdx = 0;
  noteStart = millis();
  buzzerTone(n[0].freq);
}

void updateSound() {
  if (!playing) return;
  if (millis() - noteStart < playing[playIdx].ms) return;
  playIdx++;
  if (playIdx >= playLen) { playing = nullptr; buzzerTone(0); return; }
  noteStart = millis();
  buzzerTone(playing[playIdx].freq);
}

float batteryVolts = 0;
int batteryPct = 0;
bool noBattery = true;

int percentFromCell(float v) {
  static const float V[] = {3.00f, 3.30f, 3.50f, 3.60f, 3.70f, 3.80f, 3.90f, 4.00f, 4.10f, 4.20f};
  static const int P[]   = {0, 5, 15, 30, 45, 60, 72, 84, 94, 100};
  if (v <= V[0]) return 0;
  if (v >= V[9]) return 100;
  for (int i = 1; i < 10; i++) {
    if (v <= V[i]) return P[i - 1] + (int)((v - V[i - 1]) / (V[i] - V[i - 1]) * (P[i] - P[i - 1]));
  }
  return 100;
}

void readBattery() {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) { sum += analogReadMilliVolts(P_BAT_SENSE); delay(1); }
  float pinV = (sum / 16.0f) / 1000.0f;
  batteryVolts = pinV * (100000.0f + 33000.0f) / 33000.0f;
  noBattery = batteryVolts < 2.0f;
  batteryPct = percentFromCell(batteryVolts / 2.0f);
}

enum Phase { PH_FOCUS, PH_SHORT, PH_LONG };
Phase phase = PH_FOCUS;
bool running = false;
uint32_t phaseTotalMs = 0;
uint32_t remainingMs = 0;
uint32_t lastTick = 0;
uint8_t focusDone = 0;

uint32_t durationMs(Phase p) {
  uint32_t minutes = (p == PH_FOCUS) ? FOCUS_MIN : (p == PH_SHORT) ? SHORT_BREAK_MIN : LONG_BREAK_MIN;
  return minutes * SECONDS_PER_MINUTE * 1000UL;
}

void setPhase(Phase p) {
  phase = p;
  phaseTotalMs = durationMs(p);
  remainingMs = phaseTotalMs;
  running = false;
}

void goToNextPhase(bool completed) {
  if (phase == PH_FOCUS) {
    if (completed) focusDone++;
    bool longBreak = completed && (focusDone % SESSIONS_BEFORE_LONG == 0);
    setPhase(longBreak ? PH_LONG : PH_SHORT);
  } else {
    setPhase(PH_FOCUS);
  }
}

void resetAll() {
  focusDone = 0;
  setPhase(PH_FOCUS);
}

void startTimer() {
  if (running) return;
  running = true;
  lastTick = millis();
  playMelody(MEL_START, sizeof(MEL_START) / sizeof(Note));
}

void pauseTimer() {
  running = false;
}

void updateTimer() {
  if (!running) return;
  uint32_t now = millis();
  uint32_t dt = now - lastTick;
  lastTick = now;
  if (dt >= remainingMs) {
    remainingMs = 0;
    bool wasFocus = (phase == PH_FOCUS);
    if (wasFocus) playMelody(MEL_FOCUS_DONE, sizeof(MEL_FOCUS_DONE) / sizeof(Note));
    else playMelody(MEL_BREAK_DONE, sizeof(MEL_BREAK_DONE) / sizeof(Note));
    goToNextPhase(true);
  } else {
    remainingMs -= dt;
  }
}

void updateLed() {
  bool on = false;
  if (running) {
    if (phase == PH_FOCUS) on = true;
    else on = ((millis() / 500) % 2) == 0;
  }
  digitalWrite(P_LED, on ? HIGH : LOW);
}

void drawBatteryCorner(int W) {
  if (noBattery) {
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(W - 16, 8, "USB");
    return;
  }
  int x = W - 18, y = 1;
  u8g2.drawFrame(x, y, 14, 8);
  u8g2.drawBox(x + 14, y + 2, 2, 4);
  u8g2.drawBox(x + 2, y + 2, (10 * batteryPct) / 100, 4);
}

void drawDots(int cx, int y, int r, int gap) {
  uint8_t done = (phase == PH_LONG) ? SESSIONS_BEFORE_LONG : (focusDone % SESSIONS_BEFORE_LONG);
  int total = SESSIONS_BEFORE_LONG;
  int startX = cx - ((total - 1) * gap) / 2;
  for (int i = 0; i < total; i++) {
    if (i < done) u8g2.drawDisc(startX + i * gap, y, r);
    else u8g2.drawCircle(startX + i * gap, y, r);
  }
}

void drawUI() {
  int W = u8g2.getDisplayWidth();
  int H = u8g2.getDisplayHeight();
  bool landscape = (W >= 100);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  const char* label = (phase == PH_FOCUS) ? "FOCUS" : (phase == PH_SHORT) ? "BREAK" : (landscape ? "LONG BREAK" : "LONG");
  const char* state = running ? "RUN" : (remainingMs == phaseTotalMs ? "READY" : "PAUSE");
  u8g2.drawStr(0, 9, label);
  if (landscape) u8g2.drawStr(70, 9, state);
  else u8g2.drawStr(0, 21, state);
  drawBatteryCorner(W);

  uint32_t msPerMinuteReal = SECONDS_PER_MINUTE * 1000UL;
  uint32_t totalSec = (remainingMs * 60UL + msPerMinuteReal - 1) / msPerMinuteReal;
  char buf[12];
  snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)(totalSec / 60), (unsigned)(totalSec % 60));
  u8g2.setFont(landscape ? u8g2_font_logisoso28_tn : u8g2_font_helvB14_tn);
  int tw = u8g2.getStrWidth(buf);
  int baseline = landscape ? 46 : (H / 2 + 7);
  u8g2.drawStr((W - tw) / 2, baseline, buf);

  int barY = landscape ? 50 : (H - 30);
  int barH = landscape ? 6 : 8;
  uint32_t elapsed = phaseTotalMs - remainingMs;
  int fill = (int)((uint64_t)(W - 4) * elapsed / phaseTotalMs);
  u8g2.drawFrame(1, barY, W - 2, barH);
  if (fill > 0) u8g2.drawBox(2, barY + 1, fill, barH - 2);

  if (landscape) drawDots(W / 2, 60, 2, 8);
  else drawDots(W / 2, H - 14, 3, 12);

  u8g2.sendBuffer();
}

void showMessage(const char* l1, const char* l2, const char* l3) {
  u8g2.setDisplayRotation(U8G2_R0);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 12, l1);
  u8g2.drawStr(0, 28, l2);
  u8g2.drawStr(0, 44, l3);
  u8g2.sendBuffer();
}

const u8g2_cb_t* rotationFor(OriState o) {
  switch (o) {
    case ORI_GRAV_DOWN:  return U8G2_R0;
    case ORI_GRAV_UP:    return U8G2_R2;
    case ORI_GRAV_LEFT:  return U8G2_R1;
    case ORI_GRAV_RIGHT: return U8G2_R3;
  }
  return U8G2_R0;
}
const u8g2_cb_t* currentRot = nullptr;

int waitForButton() {
  while (true) {
    btn1.update();
    btn2.update();
    if (btn1.shortEvt) return 1;
    if (btn2.shortEvt) return 2;
    delay(10);
  }
}

bool runWizard(bool allowSkip) {
  while (true) {
    showMessage("ROTATION SETUP 1/3", "Hold UPRIGHT, BTN1", allowSkip ? "(BTN2 = skip)" : "");
    if (waitForButton() == 2 && allowSkip) return false;
    AxisCfg up;
    if (!orient.captureAxis(up)) { showMessage("Not steady!", "Press BTN1", "to try again"); waitForButton(); continue; }

    showMessage("ROTATION SETUP 2/3", "Turn LEFT: right", "edge UP, then BTN1");
    waitForButton();
    AxisCfg right;
    if (!orient.captureAxis(right, up.axis)) { showMessage("Not steady!", "Press BTN1", "to start again"); waitForButton(); continue; }

    showMessage("ROTATION SETUP 3/3", "Lay FLAT, screen UP", "then press BTN1");
    waitForButton();
    AxisCfg face;
    if (!orient.captureAxis(face, up.axis, right.axis)) { showMessage("Not steady!", "Press BTN1", "to start again"); waitForButton(); continue; }

    orient.up = up;
    orient.right = right;
    orient.face = face;
    orient.save();
    showMessage("Saved!", "", "");
    delay(1000);
    return true;
  }
}

void setup() {
  Serial.begin(115200);
  uint32_t start = millis();
  while (!Serial && millis() - start < 1500) delay(10);

  pinMode(P_LED, OUTPUT);
  btn1.begin(P_BTN1);
  btn2.begin(P_BTN2);
  ledcAttach(P_BUZZER, 2000, 8);
  buzzerTone(0);
  analogSetPinAttenuation(P_BAT_SENSE, ADC_11db);

  Wire.begin(P_SDA, P_SCL);
  Wire.setClock(400000);
  u8g2.begin();
  u8g2.setBusClock(400000);

  haveMpu = orient.begin();
  if (!haveMpu) {
    Serial.println("MPU6050 not found - the timer works but the screen will not rotate");
    showMessage("No MPU6050 found", "Rotation is off", "");
    delay(1500);
  } else {
    if (btn1.stable && btn2.stable) orient.clear();
    if (orient.load()) useOrientation = true;
    else useOrientation = runWizard(true);
  }

  readBattery();
  resetAll();
  Serial.println("Pomodoro timer ready. BTN1 = start / pause");
}

FaceState prevFace = FACE_NONE;
uint32_t lastDraw = 0, lastBattery = 0, bothDownSince = 0;
bool screenOff = false;

void loop() {
  btn1.update();
  btn2.update();
  updateSound();

  if (btn1.shortEvt) {
    if (running) pauseTimer();
    else startTimer();
  }
  if (btn2.shortEvt) goToNextPhase(false);
  if (btn2.longEvt) { resetAll(); Serial.println("Reset"); }

  if (haveMpu && btn1.stable && btn2.stable) {
    if (bothDownSince == 0) bothDownSince = millis();
    if (millis() - bothDownSince > 3000) {
      bothDownSince = 0;
      orient.clear();
      useOrientation = runWizard(false);
      currentRot = nullptr;
    }
  } else {
    bothDownSince = 0;
  }

  if (useOrientation) {
    orient.update();

    const u8g2_cb_t* wanted = rotationFor(orient.ori);
    if (wanted != currentRot) {
      currentRot = wanted;
      u8g2.setDisplayRotation(currentRot);
    }

#if FLIP_TO_FOCUS
    if (prevFace != FACE_DOWN && orient.faceState == FACE_DOWN && !running) startTimer();
    if (prevFace == FACE_DOWN && orient.faceState != FACE_DOWN && running) pauseTimer();
#endif
    prevFace = orient.faceState;

    bool wantOff = (orient.faceState == FACE_DOWN);
    if (wantOff != screenOff) {
      screenOff = wantOff;
      u8g2.setPowerSave(screenOff ? 1 : 0);
    }
  }

  updateTimer();
  updateLed();

  if (millis() - lastBattery > 5000) {
    lastBattery = millis();
    readBattery();
  }

  if (!screenOff && millis() - lastDraw > 100) {
    lastDraw = millis();
    drawUI();
  }
  delay(5);
}
