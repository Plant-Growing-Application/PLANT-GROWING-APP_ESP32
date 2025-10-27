#include <Arduino.h>
#include "Pins.h"
#include "Controls.h"

// ============== ENCODER (ISR) ==============
static volatile int8_t  s_encDelta = 0;
static volatile uint8_t s_lastAB    = 0;
static volatile uint32_t s_lastUs   = 0;

static void IRAM_ATTR enc_isr() {
  uint32_t now = micros();
  if (now - s_lastUs < 800) return;  // ~0.8ms debounce
  s_lastUs = now;

  int a = digitalRead(ENCODER_A_PIN);
  int b = digitalRead(ENCODER_B_PIN);
  uint8_t curr = (a << 1) | b;
  uint8_t prev = s_lastAB;

  // İleri
  if ((prev == 0b00 && curr == 0b01) ||
      (prev == 0b01 && curr == 0b11) ||
      (prev == 0b11 && curr == 0b10) ||
      (prev == 0b10 && curr == 0b00)) {
    s_encDelta++;
  }
  // Geri
  else if ((prev == 0b00 && curr == 0b10) ||
           (prev == 0b10 && curr == 0b11) ||
           (prev == 0b11 && curr == 0b01) ||
           (prev == 0b01 && curr == 0b00)) {
    s_encDelta--;
  }
  s_lastAB = curr;
}

int inputReadEncoderDetent() {
  static int raw = 0;
  int8_t d;
  noInterrupts(); d = s_encDelta; s_encDelta = 0; interrupts();
  if (d == 0) return 0;

  raw += d;
  int det = 0;
  while (raw >= EDGES_PER_DETENT) { raw -= EDGES_PER_DETENT; det++; }
  while (raw <=-EDGES_PER_DETENT) { raw += EDGES_PER_DETENT; det--; }
  return det;
}

// ============== BUTONLAR (debounce) ==============
struct Btn {
  uint8_t pin;
  bool    lastLevel;
  uint32_t lastChangeMs;
  Btn(uint8_t p) : pin(p), lastLevel(HIGH), lastChangeMs(0) {}
};

static Btn s_btnEnc   (ENCODER_PUSH_PIN);
static Btn s_btnOK    (CONFIRM_BUTTON_PIN);
static Btn s_btnBack  (BACK_BUTTON_PIN);
static const uint16_t BTN_DEBOUNCE_MS = 30;

static bool fell(Btn &b){
  bool lvl = digitalRead(b.pin);
  uint32_t now = millis();
  if (lvl != b.lastLevel && (now - b.lastChangeMs) > BTN_DEBOUNCE_MS) {
    b.lastChangeMs = now;
    bool wasHigh = b.lastLevel;
    b.lastLevel = lvl;
    if (wasHigh && lvl == LOW) return true; // basıldı
  }
  return false;
}

bool inputButtonFell(ButtonId b){
  switch(b){
    case BTN_ENC_PUSH: return fell(s_btnEnc);
    case BTN_CONFIRM : return fell(s_btnOK);
    case BTN_BACK    : return fell(s_btnBack);
    default: return false;
  }
}

// ============== KURULUM ==============
void inputBegin() {
  pinMode(ENCODER_A_PIN,     INPUT_PULLUP);
  pinMode(ENCODER_B_PIN,     INPUT_PULLUP);
  pinMode(ENCODER_PUSH_PIN,  INPUT_PULLUP);
  pinMode(CONFIRM_BUTTON_PIN,INPUT_PULLUP);
  pinMode(BACK_BUTTON_PIN,   INPUT_PULLUP);

  s_lastAB = (digitalRead(ENCODER_A_PIN) << 1) | digitalRead(ENCODER_B_PIN);

  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), enc_isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B_PIN), enc_isr, CHANGE);
}