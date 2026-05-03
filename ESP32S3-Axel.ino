// ============================================================
//  Основной файл ESP32S3-Axel.ino
//  Поворот текста времени/температуры через tft.setRotation()
//  Синхронизация WiFi при тряске + переворот (Bottom)
//  + бездействие: через 12 сек – случайные анимации (подмигивания, взгляды),
//    через 72 сек – SLEEP, через 5 мин – выкл. подсветки
//  Исправлено: randomSeed, сброс idleStartTime при SLEEP, удалена лишняя проверка sysState
//  + Дыхание в покое (enableBreathing)
//  + Исправлен выход из сна (сброс shake и fMag)
// ============================================================

#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <TFT_eSPI.h>
#include "Eyes.h"

extern bool winkLeftActive, winkRightActive, lookLeftActive, lookRightActive;
extern void startWinkLeft(), startWinkRight(), startLookLeft(), startLookRight();
extern void enableBreathing(bool enable);

#define SDA_PIN 8
#define SCL_PIN 9
#define MPU_ADDR 0x69
#define RTC_ADDR 0x68
#define TFT_BACKLIGHT_PIN 13

const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASS";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3 * 3600;
const int daylightOffset_sec = 0;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite(&tft);

float filteredRoll = 0.0, filteredPitch = 0.0;
const float alpha = 0.2;

String currentPos = "Normal";
String stablePos = "Normal";
unsigned long posChangeTime = 0;
const unsigned long POS_HOLD_MS = 150;

enum SystemState {
  STATE_IDLE,
  STATE_ACTION,
  STATE_SHOW_TIME,
  STATE_SHOW_TEMP,
  STATE_SHAKE
};

SystemState sysState = STATE_IDLE;
unsigned long stateStartTime = 0;
int showPhase = 0;
unsigned long phaseStartTime = 0;
unsigned long lastTextUpdate = 0;
bool needBlackTransition = false;

float lastTemp = 0;
int lastH=0, lastM=0, lastS=0;

unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL_MS = 50;
const unsigned long MOVE_DURATION_MS = 230;

unsigned long lastShakeUpdate = 0;
const unsigned long SHAKE_DURATION = 3000;
const unsigned long SHAKE_INTERVAL = 50;

// ==================== ПАРАМЕТРЫ БЕЗДЕЙСТВИЯ ====================
unsigned long idleStartTime = 0;
unsigned long nextIdleAnimTime = 0;
bool idleAnimActive = false;
bool sleepyMode = false;
bool backlightOn = true;
const unsigned long IDLE_PHASE1_MS = 12000;
const unsigned long IDLE_PHASE2_MS = 72000;
const unsigned long BACKLIGHT_OFF_MS = 300000;

bool shake = false;
bool wasNeutral = false;

// ==================== ГЛОБАЛЬНАЯ ПЕРЕМЕННАЯ ДЛЯ ФИЛЬТРА (чтобы сбрасывать при пробуждении) ====================
float fMag = 1.0;

// ==================== ДАТЧИКИ ====================
byte decToBcd(byte val) { return (val / 10 * 16) + (val % 10); }
byte bcdToDec(byte val) { return (val / 16 * 10) + (val % 16); }

void readMPU(float &axg, float &ayg, float &azg, float &tempC) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);
  int16_t ax = Wire.read() << 8 | Wire.read();
  int16_t ay = Wire.read() << 8 | Wire.read();
  int16_t az = Wire.read() << 8 | Wire.read();
  int16_t tempRaw = Wire.read() << 8 | Wire.read();
  for (int i = 0; i < 6; i++) Wire.read();
  axg = ax / 16384.0;
  ayg = ay / 16384.0;
  azg = az / 16384.0;
  tempC = tempRaw / 340.0 + 36.53;
}

bool readRTC(int &h, int &m, int &s) {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0x00);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(RTC_ADDR, 3, true) != 3) return false;
  byte sec  = Wire.read();
  byte min  = Wire.read();
  byte hour = Wire.read();
  s = bcdToDec(sec & 0x7F);
  m = bcdToDec(min);
  h = bcdToDec(hour & 0x3F);
  return true;
}

void setRTCfromTM(struct tm &timeinfo) {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0x00);
  Wire.write(decToBcd(timeinfo.tm_sec) & 0x7F);
  Wire.write(decToBcd(timeinfo.tm_min));
  Wire.write(decToBcd(timeinfo.tm_hour));
  Wire.write(decToBcd(timeinfo.tm_wday + 1));
  Wire.write(decToBcd(timeinfo.tm_mday));
  Wire.write(decToBcd(timeinfo.tm_mon + 1));
  Wire.write(decToBcd(timeinfo.tm_year - 100));
  Wire.endTransmission(true);
  Serial.println("RTC time updated");
}

bool syncTimeViaWiFi() {
  const int maxAttempts = 3;
  const int delayBetween = 1000;
  for (int attempt = 1; attempt <= maxAttempts; attempt++) {
    Serial.printf("Попытка %d из %d\n", attempt, maxAttempts);
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 25) {
      delay(400);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connected");
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      struct tm timeinfo;
      int retry = 0;
      while (!getLocalTime(&timeinfo) && retry < 12) {
        delay(800);
        Serial.print(".");
        retry++;
      }
      if (getLocalTime(&timeinfo)) {
        setRTCfromTM(timeinfo);
        WiFi.disconnect(true);
        return true;
      } else Serial.println("\nNTP error");
      WiFi.disconnect(true);
    } else Serial.println("\nWiFi failed");
    if (attempt < maxAttempts) delay(delayBetween);
  }
  return false;
}

String getOrientation(float roll, float pitch, float az) {
  if (az < -0.7) return "Bottom";
  bool left = false, right = false, up = false, down = false;
  int leftLvl = 0, rightLvl = 0, upLvl = 0, downLvl = 0;
  if (roll > 10) { right = true; rightLvl = (roll > 60) ? 2 : 1; }
  else if (roll < -10) { left = true; leftLvl = (-roll > 60) ? 2 : 1; }
  if (pitch > 10) { down = true; downLvl = (pitch > 60) ? 2 : 1; }
  else if (pitch < -10) { up = true; upLvl = (-pitch > 60) ? 2 : 1; }
  if (left && up) return (leftLvl==2||upLvl==2) ? "Left-Up-60" : "Left-Up";
  if (left && down) return (leftLvl==2||downLvl==2) ? "Left-Down-60" : "Left-Down";
  if (right && up) return (rightLvl==2||upLvl==2) ? "Right-Up-60" : "Right-Up";
  if (right && down) return (rightLvl==2||downLvl==2) ? "Right-Down-60" : "Right-Down";
  if (left) return (leftLvl==2) ? "Left-60" : "Left";
  if (right) return (rightLvl==2) ? "Right-60" : "Right";
  if (up) return (upLvl==2) ? "Up-60" : "Up";
  if (down) return (downLvl==2) ? "Down-60" : "Down";
  return "Normal";
}

void applyAction(Emotion em, int dx, int dy) {
  setEmotion(em);
  setShift(dx, dy, MOVE_DURATION_MS);
  sysState = STATE_ACTION;
  stateStartTime = millis();
  idleStartTime = 0;
  nextIdleAnimTime = 0;
  idleAnimActive = false;
  enableBreathing(false);
}

void startShake() {
  setShift(0,0,0);
  lastShakeUpdate = millis();
  sysState = STATE_SHAKE;
  stateStartTime = millis();
  idleStartTime = 0;
  enableBreathing(false);
}

void startShowTime() {
  if (sysState == STATE_SHOW_TIME) return;
  sysState = STATE_SHOW_TIME;
  stateStartTime = millis();
  showPhase = 0;
  phaseStartTime = millis();
  needBlackTransition = true;
  lastTextUpdate = 0;
  setEmotion(BLACK, true);
  idleStartTime = 0;
  enableBreathing(false);
}

void startShowTemp() {
  if (sysState == STATE_SHOW_TEMP) return;
  sysState = STATE_SHOW_TEMP;
  stateStartTime = millis();
  showPhase = 0;
  phaseStartTime = millis();
  needBlackTransition = true;
  lastTextUpdate = 0;
  setEmotion(BLACK, true);
  idleStartTime = 0;
  enableBreathing(false);
}

void drawTimeOnDisplay(int h, int m) {
  static int lastH = -1, lastM = -1;
  if (h == lastH && m == lastM) return;
  lastH = h; lastM = m;
  char buf[6];
  sprintf(buf, "%02d:%02d", h, m);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(7);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(buf, tft.width()/2, tft.height()/2);
}

void drawTempOnDisplay(float temp) {
  static float lastTemp = -1000;
  if (fabs(temp - lastTemp) < 0.1) return;
  lastTemp = temp;
  char buf[12];
  snprintf(buf, sizeof(buf), "%.1f°C", temp);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(7);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(buf, tft.width()/2, tft.height()/2);
}

void showSyncResult(bool success) {
  if (success) setEmotion(EMO_OK);
  else setEmotion(EMO_FAIL);
  redrawNormal();
  sprite.pushSprite(0,0);
  delay(1000);
  setEmotion(NEUTRAL);
  redrawNormal();
  sprite.pushSprite(0,0);
  idleStartTime = 0;
  enableBreathing(false);
}

void enableBacklight(bool enable) {
#ifdef TFT_BACKLIGHT_PIN
  if (enable && !backlightOn) {
    digitalWrite(TFT_BACKLIGHT_PIN, HIGH);
    backlightOn = true;
  } else if (!enable && backlightOn) {
    digitalWrite(TFT_BACKLIGHT_PIN, LOW);
    backlightOn = false;
  }
#endif
}

void updateStateMachine() {
  unsigned long now = millis();
  bool isStrongLeft = (stablePos == "Left-60");
  bool isStrongRight = (stablePos == "Right-60");
  
  switch (sysState) {
    case STATE_IDLE:
      // Выход из спящего режима при активности
      if ((sleepyMode || !backlightOn) && (stablePos != "Normal" || shake)) {
        sleepyMode = false;
        enableBacklight(true);
        setEmotion(NEUTRAL);
        setShift(0,0,0);
        redrawNormal();
        sprite.pushSprite(0,0);
        idleStartTime = 0;
        nextIdleAnimTime = 0;
        idleAnimActive = false;
        enableBreathing(false);
        // ======== КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: сброс shake и фильтра ========
        shake = false;
        fMag = 1.0;
        // также сбросим lastShakeUpdate, чтобы не было остаточных эффектов
        lastShakeUpdate = now;
        // ===============================================================
        break;
      }
      // Если не спим – управление бездействием
      if (!sleepyMode) {
        // Активность – сбросить таймер
        if (stablePos != "Normal" || shake) {
          idleStartTime = 0;
          nextIdleAnimTime = 0;
          idleAnimActive = false;
          enableBreathing(false);
        } else {
          if (idleStartTime == 0) idleStartTime = now;
          unsigned long idleDur = now - idleStartTime;
          if (idleDur >= IDLE_PHASE1_MS && idleDur < IDLE_PHASE2_MS) {
            if (!idleAnimActive && now >= nextIdleAnimTime) {
              int r = random(0, 4);
              if (r == 0) startWinkLeft();
              else if (r == 1) startWinkRight();
              else if (r == 2) startLookLeft();
              else if (r == 3) startLookRight();
              idleAnimActive = true;
              nextIdleAnimTime = now + 5000;
              enableBreathing(false);
            }
            if (idleAnimActive && !winkLeftActive && !winkRightActive && !lookLeftActive && !lookRightActive) {
              idleAnimActive = false;
            }
          }
          if (idleDur >= IDLE_PHASE2_MS && !sleepyMode) {
            sleepyMode = true;
            idleStartTime = now;
            setEmotion(SLEEP);
            redrawNormal();
            sprite.pushSprite(0,0);
            enableBreathing(false);
          }
        }
      } else { // sleepyMode true
        if (backlightOn && (now - idleStartTime >= BACKLIGHT_OFF_MS)) {
          enableBacklight(false);
        }
        enableBreathing(false);
      }

      // Нормальная обработка наклонов и поворота экрана
      if (tft.getRotation() != 1) {
        tft.setRotation(1);
        sprite.deleteSprite();
        sprite.createSprite(tft.width(), tft.height());
        redrawNormal();
      }
      if (isStrongLeft || isStrongRight) {
        if (isStrongLeft) startShowTemp();
        else startShowTime();
        wasNeutral = false;
      } else {
        if (stablePos == "Normal") {
          if (!wasNeutral) {
            applyAction(NEUTRAL, 0, 0);
            wasNeutral = true;
          }
        } else if (stablePos == "Left") {
          applyAction(LOOK_LEFT, -55, 0);
          wasNeutral = false;
        } else if (stablePos == "Right") {
          applyAction(LOOK_RIGHT, 55, 0);
          wasNeutral = false;
        } else if (stablePos == "Left-Up") {
          applyAction(LOOK_LEFT, -55, -55);
          wasNeutral = false;
        } else if (stablePos == "Left-Down") {
          applyAction(LOOK_LEFT, -55, 55);
          wasNeutral = false;
        } else if (stablePos == "Right-Up") {
          applyAction(LOOK_RIGHT, 55, -55);
          wasNeutral = false;
        } else if (stablePos == "Right-Down") {
          applyAction(LOOK_RIGHT, 55, 55);
          wasNeutral = false;
        } else if (stablePos == "Up") {
          applyAction(NEUTRAL, 0, -45);
          wasNeutral = false;
        } else if (stablePos == "Down") {
          applyAction(NEUTRAL, 0, 45);
          wasNeutral = false;
        } else if (stablePos == "Bottom") {
          setEmotion(ALARM);
          setShift(0,0,0);
          sysState = STATE_ACTION;
          stateStartTime = now;
          wasNeutral = false;
          enableBreathing(false);
        }
      }
      
      // Включение дыхания
      if (!sleepyMode && stablePos == "Normal" && idleStartTime != 0 && (now - idleStartTime > 2000) &&
          !shake && !idleAnimActive && !winkLeftActive && !winkRightActive && 
          !lookLeftActive && !lookRightActive && currentEmotion != BLACK && currentEmotion != SLEEP) {
        enableBreathing(true);
      } else {
        static bool lastBreathingState = false;
        if (lastBreathingState == true) {
          enableBreathing(false);
          lastBreathingState = false;
        }
      }
      break;
      
    case STATE_ACTION:
      if (now - stateStartTime >= MOVE_DURATION_MS) sysState = STATE_IDLE;
      break;
      
    case STATE_SHOW_TIME:
    case STATE_SHOW_TEMP:
      {
        bool isStrong = (sysState == STATE_SHOW_TEMP) ? isStrongLeft : isStrongRight;
        int newRot = isStrongLeft ? 2 : 0;
        if (tft.getRotation() != newRot) tft.setRotation(newRot);
        if (showPhase == 0) {
          if (!transitionActive && currentEmotion == BLACK) {
            showPhase = 1; phaseStartTime = now; lastTextUpdate = 0;
            if (sysState == STATE_SHOW_TIME) drawTimeOnDisplay(lastH, lastM);
            else drawTempOnDisplay(lastTemp);
          }
        } else if (showPhase == 1) {
          if (now - lastTextUpdate >= 1000) {
            lastTextUpdate = now;
            if (sysState == STATE_SHOW_TIME) drawTimeOnDisplay(lastH, lastM);
            else drawTempOnDisplay(lastTemp);
          }
          if (!isStrong) { showPhase = 2; phaseStartTime = now; }
        } else if (showPhase == 2) {
          if (now - phaseStartTime >= 3000) {
            setEmotion(NEUTRAL, true);
            sysState = STATE_IDLE;
            setShift(0,0,0);
          } else if (isStrong) {
            showPhase = 1; phaseStartTime = now; lastTextUpdate = 0;
            if (sysState == STATE_SHOW_TIME) drawTimeOnDisplay(lastH, lastM);
            else drawTempOnDisplay(lastTemp);
          }
        }
      }
      break;
      
    case STATE_SHAKE:
      if (now - lastShakeUpdate >= SHAKE_INTERVAL) {
        lastShakeUpdate = now;
        setShift(random(-8,9), random(-8,9), SHAKE_INTERVAL/2);
      }
      if (now - stateStartTime >= SHAKE_DURATION) {
        sysState = STATE_IDLE;
        setShift(0,0,0);
        idleStartTime = 0;
      }
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("System start");
  randomSeed(esp_random());

#ifdef TFT_BACKLIGHT_PIN
  pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(TFT_BACKLIGHT_PIN, HIGH);
#endif
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  sprite.createSprite(tft.width(), tft.height());
  initEyes();
  setEmotion(NEUTRAL);
  setShift(0,0,0);
  wasNeutral = true;
  enableBreathing(false);
}

void loop() {
  unsigned long now = millis();
  if (now - lastSensorRead >= SENSOR_INTERVAL_MS) {
    lastSensorRead = now;
    float ax, ay, az, temp;
    readMPU(ax, ay, az, temp);
    lastTemp = temp;
    float roll = atan2(ay, az) * 57.2958;
    float pitch = atan2(-ax, sqrt(ay*ay+az*az)) * 57.2958;
    filteredRoll = filteredRoll * (1-alpha) + roll * alpha;
    filteredPitch = filteredPitch * (1-alpha) + pitch * alpha;
    int h,m,s;
    bool rtcOk = readRTC(h,m,s);
    if (rtcOk) { lastH = h; lastM = m; lastS = s; }
    String rawPos = getOrientation(filteredRoll, filteredPitch, az);
    float mag = sqrt(ax*ax+ay*ay+az*az);
    // fMag теперь глобальная
    fMag = 0.7 * fMag + 0.3 * mag;
    shake = (fabs(fMag - 1.0) > 0.6);
    if (shake) rawPos = "Shake";
    if (rawPos != currentPos) {
      currentPos = rawPos;
      posChangeTime = now;
    }
    if (now - posChangeTime >= POS_HOLD_MS) stablePos = currentPos;
    static unsigned long lastSync = 0;
    if (shake && stablePos == "Bottom" && (now - lastSync > 10000)) {
      if (sysState != STATE_SHOW_TIME && sysState != STATE_SHOW_TEMP && sysState != STATE_SHAKE) {
        startShake();
        delay(50);
        bool ok = syncTimeViaWiFi();
        showSyncResult(ok);
        lastSync = now;
      }
    } else if (shake && sysState != STATE_SHOW_TIME && sysState != STATE_SHOW_TEMP && sysState != STATE_SHAKE) {
      startShake();
    }
    static unsigned long lastSerial = 0;
    if (now - lastSerial >= 1000) {
      lastSerial = now;
      char tbuf[9];
      if (rtcOk) sprintf(tbuf,"%02d:%02d:%02d",lastH,lastM,lastS);
      else strcpy(tbuf,"??:??:??");
      Serial.printf("Время: %s | Температура: %.1f C | Положение: %s | Состояние: %d\n",
                    tbuf, lastTemp, stablePos.c_str(), sysState);
    }
  }
  updateEyes();
  updateStateMachine();
  if (sysState != STATE_SHOW_TIME && sysState != STATE_SHOW_TEMP) {
    sprite.pushSprite(0,0);
  }
}
