#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <math.h>
#include <Preferences.h>

#define P_SDA  D4
#define P_SCL  D5
#define P_BTN1 D6
#define P_BTN2 D7

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

const u8g2_cb_t* rotationFor(OriState o) {
  switch (o) {
    case ORI_GRAV_DOWN:  return U8G2_R0;
    case ORI_GRAV_UP:    return U8G2_R2;
    case ORI_GRAV_LEFT:  return U8G2_R1;
    case ORI_GRAV_RIGHT: return U8G2_R3;
  }
  return U8G2_R0;
}

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

void showMessage(const char* l1, const char* l2, const char* l3) {
  u8g2.setDisplayRotation(U8G2_R0);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 12, l1);
  u8g2.drawStr(0, 28, l2);
  u8g2.drawStr(0, 44, l3);
  u8g2.sendBuffer();
}

void waitForBtn1() {
  while (true) {
    btn1.update();
    if (btn1.pressedEvt) return;
    delay(10);
  }
}

bool runWizard() {
  Serial.println("Calibration wizard started");
  while (true) {
    showMessage("STEP 1 of 3", "Hold timer UPRIGHT", "then press BTN1");
    waitForBtn1();
    AxisCfg up;
    if (!orient.captureAxis(up)) { showMessage("Not steady!", "Hold still,", "press BTN1 to retry"); waitForBtn1(); continue; }

    showMessage("STEP 2 of 3", "Turn it LEFT so the", "right edge is UP. BTN1");
    waitForBtn1();
    AxisCfg right;
    if (!orient.captureAxis(right, up.axis)) { showMessage("Not steady!", "Start again", "press BTN1"); waitForBtn1(); continue; }

    showMessage("STEP 3 of 3", "Lay it FLAT, screen", "facing UP. BTN1");
    waitForBtn1();
    AxisCfg face;
    if (!orient.captureAxis(face, up.axis, right.axis)) { showMessage("Not steady!", "Start again", "press BTN1"); waitForBtn1(); continue; }

    orient.up = up;
    orient.right = right;
    orient.face = face;
    orient.save();
    Serial.printf("Saved: up=%d%+d right=%d%+d face=%d%+d\n",
                  up.axis, up.sign, right.axis, right.sign, face.axis, face.sign);
    showMessage("Calibration saved!", "Try rotating me", "");
    delay(1500);
    return true;
  }
}

bool debugView = false;
const u8g2_cb_t* currentRot = U8G2_R0;

void drawMain() {
  int W = u8g2.getDisplayWidth();
  int H = u8g2.getDisplayHeight();
  u8g2.clearBuffer();

  if (orient.faceState == FACE_DOWN) {
    u8g2.setFont(u8g2_font_helvB14_tr);
    const char* t = "FLIPPED";
    u8g2.drawStr((W - u8g2.getStrWidth(t)) / 2, H / 2 + 6, t);
    u8g2.sendBuffer();
    return;
  }

  int cx = W / 2;
  u8g2.drawTriangle(cx, 14, cx - 12, 30, cx + 12, 30);
  u8g2.drawBox(cx - 5, 30, 10, 16);

  u8g2.setFont(u8g2_font_6x10_tf);
  const char* name = orient.oriName();
  u8g2.drawStr((W - u8g2.getStrWidth(name)) / 2, 11, name);
  const char* hint = "I stay upright!";
  u8g2.drawStr((W - u8g2.getStrWidth(hint)) / 2, H - 4, hint);

  if (debugView) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%+.1f %+.1f %+.1f", orient.a[0], orient.a[1], orient.a[2]);
    u8g2.drawStr(0, H - 14, buf);
  }
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  uint32_t start = millis();
  while (!Serial && millis() - start < 1500) delay(10);

  btn1.begin(P_BTN1);
  btn2.begin(P_BTN2);
  Wire.begin(P_SDA, P_SCL);
  Wire.setClock(400000);
  u8g2.begin();
  u8g2.setBusClock(400000);

  if (!orient.begin()) {
    showMessage("MPU6050 not found", "check the socket", "then reset");
    Serial.println("MPU6050 not found");
    while (true) delay(1000);
  }

  if (btn1.stable && btn2.stable) {
    orient.clear();
    Serial.println("Calibration erased");
  }

  if (!orient.load()) runWizard();
  else Serial.println("Calibration loaded from flash");
}

uint32_t bothDownSince = 0;
uint32_t lastDraw = 0;

void loop() {
  btn1.update();
  btn2.update();
  orient.update();

  if (btn1.pressedEvt) debugView = !debugView;

  if (btn1.stable && btn2.stable) {
    if (bothDownSince == 0) bothDownSince = millis();
    if (millis() - bothDownSince > 3000) {
      bothDownSince = 0;
      orient.clear();
      runWizard();
      currentRot = nullptr;
    }
  } else {
    bothDownSince = 0;
  }

  const u8g2_cb_t* wanted = rotationFor(orient.ori);
  if (wanted != currentRot) {
    currentRot = wanted;
    u8g2.setDisplayRotation(currentRot);
    Serial.printf("Rotation changed: %s\n", orient.oriName());
  }

  if (millis() - lastDraw > 40) {
    lastDraw = millis();
    drawMain();
  }
  delay(10);
}
