#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <U8g2lib.h>

#define P_SDA     D4
#define P_SCL     D5
#define P_BTN1    D6
#define P_BTN2    D7
#define P_MPU_INT D3
#define P_LED     D10

#define MPU_ADDR 0x68

#define REG_SMPLRT_DIV    0x19
#define REG_CONFIG        0x1A
#define REG_GYRO_CONFIG   0x1B
#define REG_ACCEL_CONFIG  0x1C
#define REG_MOT_THR       0x1F
#define REG_MOT_DUR       0x20
#define REG_INT_PIN_CFG   0x37
#define REG_INT_ENABLE    0x38
#define REG_INT_STATUS    0x3A
#define REG_ACCEL_XOUT_H  0x3B
#define REG_MOT_DETECT    0x69
#define REG_PWR_MGMT_1    0x6B
#define REG_WHO_AM_I      0x75

const char* const ACCEL_NAMES[4] = {"+-2g", "+-4g", "+-8g", "+-16g"};
const float ACCEL_LSB[4] = {16384.0f, 8192.0f, 4096.0f, 2048.0f};
const char* const GYRO_NAMES[4] = {"250", "500", "1000", "2000"};
const float GYRO_LSB[4] = {131.0f, 65.5f, 32.8f, 16.4f};
const uint8_t RATE_DIVS[5] = {0, 4, 9, 19, 49};

struct Settings {
  uint8_t accelRange = 0;
  uint8_t gyroRange = 0;
  uint8_t dlpf = 3;
  uint8_t rateIdx = 2;
  bool motionInt = false;
} cfg;

const uint8_t MOTION_THRESHOLD = 8;

float gyroBias[3] = {0, 0, 0};
bool plotterMode = false;
bool bubbleView = false;
bool mpuOk = false;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

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

void writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint8_t readReg(uint8_t reg) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((int)MPU_ADDR, (int)1);
  return Wire.available() ? Wire.read() : 0;
}

bool readBytes(uint8_t reg, uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)MPU_ADDR, (int)n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

void applySettings() {
  writeReg(REG_PWR_MGMT_1, 0x01);
  writeReg(REG_CONFIG, cfg.dlpf & 0x07);
  writeReg(REG_SMPLRT_DIV, RATE_DIVS[cfg.rateIdx]);
  writeReg(REG_GYRO_CONFIG, cfg.gyroRange << 3);

  uint8_t accelCfg = cfg.accelRange << 3;
  if (cfg.motionInt) {
    accelCfg |= 0x01;
    writeReg(REG_ACCEL_CONFIG, accelCfg);
    writeReg(REG_MOT_THR, MOTION_THRESHOLD);
    writeReg(REG_MOT_DUR, 1);
    writeReg(REG_MOT_DETECT, 0x15);
    writeReg(REG_INT_PIN_CFG, 0x20);
    writeReg(REG_INT_ENABLE, 0x40);
  } else {
    writeReg(REG_ACCEL_CONFIG, accelCfg);
    writeReg(REG_INT_ENABLE, 0x00);
  }
  readReg(REG_INT_STATUS);
}

int actualRateHz() {
  int base = (cfg.dlpf == 0 || cfg.dlpf == 7) ? 8000 : 1000;
  return base / (1 + RATE_DIVS[cfg.rateIdx]);
}

bool mpuBegin() {
  Wire.beginTransmission(MPU_ADDR);
  if (Wire.endTransmission() != 0) return false;
  writeReg(REG_PWR_MGMT_1, 0x80);
  delay(100);
  applySettings();
  delay(50);
  return true;
}

struct Sample {
  float ax, ay, az;
  float gx, gy, gz;
  float tempC;
  float roll, pitch;
};

bool readSample(Sample& s) {
  uint8_t b[14];
  if (!readBytes(REG_ACCEL_XOUT_H, b, 14)) return false;
  int16_t rax = (b[0] << 8) | b[1];
  int16_t ray = (b[2] << 8) | b[3];
  int16_t raz = (b[4] << 8) | b[5];
  int16_t rt  = (b[6] << 8) | b[7];
  int16_t rgx = (b[8] << 8) | b[9];
  int16_t rgy = (b[10] << 8) | b[11];
  int16_t rgz = (b[12] << 8) | b[13];

  float aLsb = ACCEL_LSB[cfg.accelRange];
  float gLsb = GYRO_LSB[cfg.gyroRange];
  s.ax = rax / aLsb;
  s.ay = ray / aLsb;
  s.az = raz / aLsb;
  s.gx = rgx / gLsb - gyroBias[0];
  s.gy = rgy / gLsb - gyroBias[1];
  s.gz = rgz / gLsb - gyroBias[2];
  s.tempC = rt / 340.0f + 36.53f;
  s.roll = atan2f(s.ay, s.az) * 180.0f / PI;
  s.pitch = atan2f(-s.ax, sqrtf(s.ay * s.ay + s.az * s.az)) * 180.0f / PI;
  return true;
}

void calibrateGyro() {
  Serial.println("Calibrating gyro - keep the board still for 2 seconds...");
  const int N = 200;
  float sum[3] = {0, 0, 0};
  gyroBias[0] = gyroBias[1] = gyroBias[2] = 0;
  Sample s;
  int got = 0;
  for (int i = 0; i < N; i++) {
    if (readSample(s)) {
      sum[0] += s.gx; sum[1] += s.gy; sum[2] += s.gz;
      got++;
    }
    delay(10);
  }
  if (got > 0) {
    for (int i = 0; i < 3; i++) gyroBias[i] = sum[i] / got;
    Serial.printf("Gyro bias removed: %.2f %.2f %.2f deg/s\n", gyroBias[0], gyroBias[1], gyroBias[2]);
  }
}

void printSettings() {
  Serial.printf("Settings: accel %s | gyro %s dps | DLPF %u | rate %d Hz | motion int %s\n",
                ACCEL_NAMES[cfg.accelRange], GYRO_NAMES[cfg.gyroRange], cfg.dlpf,
                actualRateHz(), cfg.motionInt ? "ON" : "off");
}

void nextAccel() { cfg.accelRange = (cfg.accelRange + 1) % 4; applySettings(); printSettings(); }
void nextGyro()  { cfg.gyroRange = (cfg.gyroRange + 1) % 4;   applySettings(); printSettings(); }
void nextDlpf()  { cfg.dlpf = (cfg.dlpf + 1) % 7;             applySettings(); printSettings(); }
void nextRate()  { cfg.rateIdx = (cfg.rateIdx + 1) % 5;       applySettings(); printSettings(); }
void toggleMotion() { cfg.motionInt = !cfg.motionInt;         applySettings(); printSettings(); }

void printHelp() {
  Serial.println("Commands: a=accel range  g=gyro range  d=filter  r=sample rate");
  Serial.println("          m=motion interrupt  z=calibrate gyro  p=plotter/text  h=help");
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'a': nextAccel(); break;
      case 'g': nextGyro(); break;
      case 'd': nextDlpf(); break;
      case 'r': nextRate(); break;
      case 'm': toggleMotion(); break;
      case 'z': calibrateGyro(); break;
      case 'p': plotterMode = !plotterMode; break;
      case 'h': printHelp(); break;
      default: break;
    }
  }
}

void drawNumbers(const Sample& s) {
  char line[32];
  u8g2.setFont(u8g2_font_6x10_tf);
  snprintf(line, sizeof(line), "A %+.2f %+.2f %+.2f", s.ax, s.ay, s.az);
  u8g2.drawStr(0, 9, line);
  snprintf(line, sizeof(line), "G %+4.0f %+4.0f %+4.0f", s.gx, s.gy, s.gz);
  u8g2.drawStr(0, 21, line);
  snprintf(line, sizeof(line), "Roll%+4.0f Pitch%+4.0f", s.roll, s.pitch);
  u8g2.drawStr(0, 33, line);
  snprintf(line, sizeof(line), "%s DLPF%u %dHz", ACCEL_NAMES[cfg.accelRange], cfg.dlpf, actualRateHz());
  u8g2.drawStr(0, 45, line);
  snprintf(line, sizeof(line), "T%.1fC mot:%s", s.tempC, cfg.motionInt ? "ON" : "off");
  u8g2.drawStr(0, 57, line);
}

void drawBubble(const Sample& s) {

  int cx = 64, cy = 36, R = 24;
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "Bubble level");
  u8g2.drawCircle(cx, cy, R);
  u8g2.drawHLine(cx - 4, cy, 9);
  u8g2.drawVLine(cx, cy - 4, 9);
  float dx = constrain(s.ax, -1.0f, 1.0f) * R;
  float dy = constrain(s.ay, -1.0f, 1.0f) * R;
  u8g2.drawDisc(cx + (int)dx, cy + (int)dy, 4);
}

void updateScreen(const Sample& s) {
  u8g2.clearBuffer();
  if (bubbleView) drawBubble(s);
  else drawNumbers(s);
  u8g2.sendBuffer();
}

void drawMessage(const char* l1, const char* l2) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 24, l1);
  u8g2.drawStr(0, 38, l2);
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  uint32_t start = millis();
  while (!Serial && millis() - start < 3000) delay(10);

  pinMode(P_LED, OUTPUT);
  pinMode(P_MPU_INT, INPUT);
  btn1.begin(P_BTN1);
  btn2.begin(P_BTN2);

  Wire.begin(P_SDA, P_SCL);
  Wire.setClock(400000);
  u8g2.begin();
  u8g2.setBusClock(400000);

  mpuOk = mpuBegin();
  if (!mpuOk) {
    Serial.println("MPU6050 not found! Run 02_i2c_scanner and check the socket.");
    drawMessage("MPU6050 not found", "check the socket");
    return;
  }
  Serial.printf("MPU6050 found, WHO_AM_I = 0x%02X\n", readReg(REG_WHO_AM_I));
  printSettings();
  printHelp();
  calibrateGyro();
}

uint32_t lastScreen = 0, lastPrint = 0;

void loop() {
  btn1.update();
  btn2.update();
  if (!mpuOk) {
    delay(2000);
    mpuOk = mpuBegin();
    return;
  }

  if (btn1.shortEvt) nextAccel();
  if (btn2.shortEvt) nextDlpf();
  if (btn1.longEvt) bubbleView = !bubbleView;
  handleSerial();

  Sample s;
  if (!readSample(s)) {
    Serial.println("read error");
    delay(100);
    return;
  }

  bool moved = false;
  if (cfg.motionInt && digitalRead(P_MPU_INT) == HIGH) {
    moved = true;
    readReg(REG_INT_STATUS);
    Serial.println(">>> MOTION detected!");
  }
  digitalWrite(P_LED, moved ? HIGH : LOW);

  if (millis() - lastScreen > 60) {
    lastScreen = millis();
    updateScreen(s);
  }

  if (millis() - lastPrint > 200) {
    lastPrint = millis();
    if (plotterMode) {

      Serial.printf("ax:%.2f,ay:%.2f,az:%.2f,roll:%.0f,pitch:%.0f\n", s.ax, s.ay, s.az, s.roll, s.pitch);
    } else {
      Serial.printf("acc[g] %+.2f %+.2f %+.2f | gyro[dps] %+6.1f %+6.1f %+6.1f | roll %+5.1f pitch %+5.1f | %.1fC\n",
                    s.ax, s.ay, s.az, s.gx, s.gy, s.gz, s.roll, s.pitch, s.tempC);
    }
  }
}
