#ifndef Eyes_h
#define Eyes_h

#include <TFT_eSPI.h>
#include <math.h>

extern TFT_eSprite sprite;

struct EyeParams {
  int leftWidth;
  int leftHeight;
  int leftOffsetX;
  int leftOffsetY;
  int rightWidth;
  int rightHeight;
  int rightOffsetX;
  int rightOffsetY;
  int spacing;
  int cornerRadius;
  uint16_t color;
};

// ==================== ENUM ЭМОЦИЙ ====================
enum Emotion {
  NEUTRAL,
  LOOK_LEFT,
  LOOK_RIGHT,
  SLEEP,
  ALARM,
  ANGRY,
  HAPPY,
  SAD,
  SURPRISED,
  BLACK,
  EMO_OK,
  EMO_FAIL
};

// ==================== ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ (ВСЕ ОБЪЯВЛЕНЫ ЗДЕСЬ) ====================
int baseLeftX = 0, baseRightX = 0, baseY = 0;

EyeParams current;
EyeParams target;
bool transitionActive = false;
unsigned long transitionStart = 0;
unsigned long transitionDuration = 500;

int currentShiftX = 0, currentShiftY = 0;
int targetShiftX = 0, targetShiftY = 0;
bool shiftTransitionActive = false;
unsigned long shiftTransitionStart = 0;
unsigned long shiftTransitionDuration = 300;

// Дыхание
bool breathingEnabled = false;
float currentBreathShift = 0.0f;
unsigned long lastBreathUpdate = 0;
const float BREATH_AMPLITUDE = 2.0f;
const float BREATH_PERIOD_MS = 3000.0f;

// Временные взгляды
bool lookLeftActive = false;
unsigned long lookLeftStart = 0;
Emotion savedLookEmotionLeft;
int savedShiftXLeft, savedShiftYLeft;

bool lookRightActive = false;
unsigned long lookRightStart = 0;
Emotion savedLookEmotionRight;
int savedShiftXRight, savedShiftYRight;

// Моргание двустороннее
bool blinking = false;
unsigned long blinkStart = 0;
unsigned long lastBlink = 0;
EyeParams savedBeforeBlink;

// Подмигивание левым глазом
bool winkLeftActive = false;
unsigned long winkLeftStart = 0;
EyeParams savedBeforeWinkLeft;

// Подмигивание правым глазом
bool winkRightActive = false;
unsigned long winkRightStart = 0;
EyeParams savedBeforeWinkRight;

// Текущая эмоция
Emotion currentEmotion = NEUTRAL;

// ==================== ТАБЛИЦА ПАРАМЕТРОВ ЭМОЦИЙ ====================
const EyeParams emotionParams[] = {
  { 50, 50, 0, 0, 50, 50, 0, 0, 18, 8, TFT_WHITE },
  { 50, 80, 0, -15, 50, 50, 0, 0, 18, 8, TFT_WHITE },
  { 50, 50, 0, 0, 50, 80, 0, -15, 18, 8, TFT_WHITE },
  { 50, 4, 0, 23, 50, 4, 0, 23, 18, 8, TFT_WHITE },
  { 60, 60, -5, -5, 60, 60, -5, -5, 18, 20, TFT_RED },
  { 50, 50, 0, 0, 50, 50, 0, 0, 18, 8, TFT_RED },
  { 50, 50, 0, -5, 50, 50, 0, -5, 18, 8, TFT_YELLOW },
  { 50, 50, 0, 5, 50, 50, 0, 5, 18, 8, TFT_BLUE },
  { 50, 70, 0, -10, 50, 70, 0, -10, 18, 8, TFT_CYAN },
  { 50, 50, 0, 0, 50, 50, 0, 0, 18, 8, TFT_BLACK },
  { 30, 50, 10, 0, 30, 50, 10, 0, 18, 6, TFT_GREEN },
  { 30, 50, 10, 0, 30, 50, 10, 0, 18, 6, TFT_RED }
};

// ==================== БАЗОВЫЕ ФУНКЦИИ ====================
void initEyeCoordinates() {
  int W = sprite.width();
  int H = sprite.height();
  int totalWidth = emotionParams[0].leftWidth + emotionParams[0].rightWidth + emotionParams[0].spacing;
  baseLeftX = (W - totalWidth) / 2;
  baseRightX = baseLeftX + emotionParams[0].leftWidth + emotionParams[0].spacing;
  baseY = (H - emotionParams[0].leftHeight) / 2;
}

void applyCurrentParams() {
  int breathShiftInt = (int)round(currentBreathShift);
  int leftX = baseLeftX + current.leftOffsetX + currentShiftX;
  int rightX = baseRightX + current.rightOffsetX + currentShiftX;
  int leftY = baseY + current.leftOffsetY + currentShiftY + breathShiftInt;
  int rightY = baseY + current.rightOffsetY + currentShiftY + breathShiftInt;
  sprite.fillRoundRect(leftX, leftY, current.leftWidth, current.leftHeight, current.cornerRadius, current.color);
  sprite.fillRoundRect(rightX, rightY, current.rightWidth, current.rightHeight, current.cornerRadius, current.color);
}

void redrawNormal() {
  sprite.fillScreen(TFT_BLACK);
  applyCurrentParams();
}

// ==================== ДЫХАНИЕ ====================
void enableBreathing(bool enable) {
  if (!enable) {
    currentBreathShift = 0.0f;
    breathingEnabled = false;
  } else {
    breathingEnabled = true;
    lastBreathUpdate = millis();
  }
}

void updateBreathing() {
  if (!breathingEnabled) return;
  unsigned long now = millis();
  float angle = 2.0f * 3.14159265f * (now % (unsigned long)BREATH_PERIOD_MS) / BREATH_PERIOD_MS;
  currentBreathShift = BREATH_AMPLITUDE * sin(angle);
}

// ==================== ПЛАВНЫЙ ПЕРЕХОД ЭМОЦИЙ ====================
void startTransition(Emotion newEmotion, unsigned long durationMs = 500) {
  target = emotionParams[newEmotion];
  transitionActive = true;
  transitionStart = millis();
  transitionDuration = durationMs;
}

void updateTransition() {
  if (!transitionActive) return;
  // Не мешать подмигиванию
  if (winkLeftActive || winkRightActive) return;
  unsigned long now = millis();
  unsigned long elapsed = now - transitionStart;
  if (elapsed >= transitionDuration) {
    current = target;
    transitionActive = false;
    redrawNormal();
    return;
  }
  float t = (float)elapsed / transitionDuration;
  EyeParams newParams;
  newParams.leftWidth     = current.leftWidth     + (target.leftWidth     - current.leftWidth)     * t;
  newParams.leftHeight    = current.leftHeight    + (target.leftHeight    - current.leftHeight)    * t;
  newParams.leftOffsetX   = current.leftOffsetX   + (target.leftOffsetX   - current.leftOffsetX)   * t;
  newParams.leftOffsetY   = current.leftOffsetY   + (target.leftOffsetY   - current.leftOffsetY)   * t;
  newParams.rightWidth    = current.rightWidth    + (target.rightWidth    - current.rightWidth)    * t;
  newParams.rightHeight   = current.rightHeight   + (target.rightHeight   - current.rightHeight)   * t;
  newParams.rightOffsetX  = current.rightOffsetX  + (target.rightOffsetX  - current.rightOffsetX)  * t;
  newParams.rightOffsetY  = current.rightOffsetY  + (target.rightOffsetY  - current.rightOffsetY)  * t;
  newParams.spacing       = current.spacing       + (target.spacing       - current.spacing)       * t;
  newParams.cornerRadius  = current.cornerRadius  + (target.cornerRadius  - current.cornerRadius)  * t;
  uint8_t r1 = (current.color >> 11) & 0x1F;
  uint8_t g1 = (current.color >> 5) & 0x3F;
  uint8_t b1 = current.color & 0x1F;
  uint8_t r2 = (target.color >> 11) & 0x1F;
  uint8_t g2 = (target.color >> 5) & 0x3F;
  uint8_t b2 = target.color & 0x1F;
  uint8_t r = r1 + (r2 - r1) * t;
  uint8_t g = g1 + (g2 - g1) * t;
  uint8_t b = b1 + (b2 - b1) * t;
  newParams.color = (r << 11) | (g << 5) | b;
  current = newParams;
  redrawNormal();
}

// ==================== СМЕЩЕНИЕ ГЛАЗ ====================
void setShift(int x, int y, unsigned long durationMs = 300) {
  targetShiftX = x;
  targetShiftY = y;
  if (durationMs == 0) {
    currentShiftX = x;
    currentShiftY = y;
    redrawNormal();
    shiftTransitionActive = false;
  } else {
    shiftTransitionActive = true;
    shiftTransitionStart = millis();
    shiftTransitionDuration = durationMs;
  }
}

void updateShift() {
  if (!shiftTransitionActive) return;
  if (winkLeftActive || winkRightActive) return;
  unsigned long now = millis();
  unsigned long elapsed = now - shiftTransitionStart;
  if (elapsed >= shiftTransitionDuration) {
    currentShiftX = targetShiftX;
    currentShiftY = targetShiftY;
    shiftTransitionActive = false;
    redrawNormal();
    return;
  }
  float t = (float)elapsed / shiftTransitionDuration;
  currentShiftX = currentShiftX + (targetShiftX - currentShiftX) * t;
  currentShiftY = currentShiftY + (targetShiftY - currentShiftY) * t;
  redrawNormal();
}

// ==================== УПРАВЛЕНИЕ ЭМОЦИЯМИ ====================
void setEmotion(Emotion em, bool smooth = true) {
  currentEmotion = em;
  if (smooth) {
    startTransition(em);
  } else {
    transitionActive = false;
    current = emotionParams[em];
    redrawNormal();
  }
}

// ==================== МОРГАНИЕ (оба глаза) ====================
void updateBlink() {
  if (currentEmotion == SLEEP || currentEmotion == BLACK) return;
  if (lookLeftActive || lookRightActive) return;
  if (winkLeftActive || winkRightActive) return; // не мешать подмигиванию
  if (!blinking && (millis() - lastBlink > 3000)) {
    blinking = true;
    blinkStart = millis();
    savedBeforeBlink = current;
    current.leftHeight = 5;
    current.rightHeight = 5;
    redrawNormal();
  } else if (blinking && (millis() - blinkStart >= 150)) {
    blinking = false;
    lastBlink = millis();
    current = savedBeforeBlink;
    redrawNormal();
  }
}

// ==================== ПОДМИГИВАНИЕ ЛЕВЫМ ГЛАЗОМ ====================
void startWinkLeft() {
  if (winkLeftActive) return;
  transitionActive = false;
  shiftTransitionActive = false;
  winkLeftActive = true;
  winkLeftStart = millis();
  savedBeforeWinkLeft = current;
  current.leftHeight = 5;
  redrawNormal();
}

void updateWinkLeft() {
  if (!winkLeftActive) return;
  if (lookLeftActive || lookRightActive) return;
  if (millis() - winkLeftStart >= 150) {
    winkLeftActive = false;
    current = savedBeforeWinkLeft;
    redrawNormal();
  }
}

// ==================== ПОДМИГИВАНИЕ ПРАВЫМ ГЛАЗОМ ====================
void startWinkRight() {
  if (winkRightActive) return;
  transitionActive = false;
  shiftTransitionActive = false;
  winkRightActive = true;
  winkRightStart = millis();
  savedBeforeWinkRight = current;
  current.rightHeight = 5;
  redrawNormal();
}

void updateWinkRight() {
  if (!winkRightActive) return;
  if (lookLeftActive || lookRightActive) return;
  if (millis() - winkRightStart >= 150) {
    winkRightActive = false;
    current = savedBeforeWinkRight;
    redrawNormal();
  }
}

// ==================== ВРЕМЕННЫЙ ВЗГЛЯД ВЛЕВО ====================
void startLookLeft() {
  if (lookLeftActive) return;
  transitionActive = false;
  shiftTransitionActive = false;
  lookLeftActive = true;
  lookLeftStart = millis();
  savedLookEmotionLeft = currentEmotion;
  savedShiftXLeft = currentShiftX;
  savedShiftYLeft = currentShiftY;
  setEmotion(LOOK_LEFT, false);
  setShift(-30, 0, 200);
}

void updateLookLeft() {
  if (!lookLeftActive) return;
  if (millis() - lookLeftStart >= 500) {
    lookLeftActive = false;
    setEmotion(savedLookEmotionLeft, true);
    setShift(savedShiftXLeft, savedShiftYLeft, 200);
  }
}

// ==================== ВРЕМЕННЫЙ ВЗГЛЯД ВПРАВО ====================
void startLookRight() {
  if (lookRightActive) return;
  transitionActive = false;
  shiftTransitionActive = false;
  lookRightActive = true;
  lookRightStart = millis();
  savedLookEmotionRight = currentEmotion;
  savedShiftXRight = currentShiftX;
  savedShiftYRight = currentShiftY;
  setEmotion(LOOK_RIGHT, false);
  setShift(30, 0, 200);
}

void updateLookRight() {
  if (!lookRightActive) return;
  if (millis() - lookRightStart >= 500) {
    lookRightActive = false;
    setEmotion(savedLookEmotionRight, true);
    setShift(savedShiftXRight, savedShiftYRight, 200);
  }
}

// ==================== ИНИЦИАЛИЗАЦИЯ ====================
void initEyes() {
  initEyeCoordinates();
  current = emotionParams[NEUTRAL];
  target = current;
  currentShiftX = 0; currentShiftY = 0;
  targetShiftX = 0; targetShiftY = 0;
  breathingEnabled = false;
  currentBreathShift = 0;
  redrawNormal();
  lastBlink = millis();
}

// ==================== ОБНОВЛЕНИЕ В ЦИКЛЕ ====================
void updateEyes() {
  updateTransition();
  updateShift();
  updateBreathing();
  updateBlink();
  updateWinkLeft();
  updateWinkRight();
  updateLookLeft();
  updateLookRight();
}

#endif