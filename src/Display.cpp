#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "Pins.h"     // I2C_SDA, I2C_SCL, OLED_ADDR, OLED_W, OLED_H
#include "Display.h"

static Adafruit_SSD1306 s_disp(OLED_W, OLED_H, &Wire, -1);

// --- Toast durumu ---
static bool     s_toastActive = false;
static String   s_toastText;
static uint32_t s_toastEndMs  = 0;

// Yardımcı: ortalanmış yazı
static void drawCentered(const String& s, uint8_t size = 2, int y = 20) {
  s_disp.clearDisplay();
  s_disp.setTextSize(size);
  s_disp.setTextColor(SSD1306_WHITE);
  int16_t x1, y1; uint16_t w, h;
  s_disp.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  int x = (OLED_W - (int)w) / 2;
  if (x < 0) x = 0;
  s_disp.setCursor(x, y);
  s_disp.print(s);
  s_disp.display();
}

bool displayBegin() {
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!s_disp.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("[display] SSD1306 baslatilamadi."));
    return false;
  }
  s_disp.clearDisplay();
  s_disp.display();
  s_toastActive = false;
  return true;
}

// ----- Genel menü (gerekirse) -----
void displayDrawPage(int page) {
  if (s_toastActive) return; // Toast varken menü çizme, mesaj üstte kalsın

  s_disp.clearDisplay();
  s_disp.setTextColor(SSD1306_WHITE);

  s_disp.setTextSize(2);
  s_disp.setCursor(6, 2);
  s_disp.print("MENU");

  s_disp.setTextSize(1);
  s_disp.setCursor(0, 22);
  s_disp.print("Sayfa: "); s_disp.print(page + 1);

  s_disp.setCursor(0, 38);
  if (page == 0)      s_disp.print("Icerik: Sayfa 1");
  else if (page == 1) s_disp.print("Icerik: Sayfa 2");
  else                s_disp.print("Icerik: Sayfa 3");

  s_disp.setCursor(0, 54);
  s_disp.print("Enc:Sayfa  Push:Edit  OK/Back");
  s_disp.display();
}

// ----- Non-blocking toast -----
void displayShowToast(const char* msg, uint16_t ms, bool replace) {
  if (s_toastActive && !replace) return; // Mevcut toasta dokunma

  const uint32_t now = millis();
  s_toastText   = String(msg);
  s_toastActive = true;
  s_toastEndMs  = now + ms;

  drawCentered(s_toastText, 2, 20);
}

void displayTick(int currentPage) {
  if (!s_toastActive) return;
  const int32_t diff = (int32_t)(millis() - s_toastEndMs);
  if (diff >= 0) {
    s_toastActive = false;
    displayDrawPage(currentPage); // menüye geri
  }
}

// ===== ÖZEL SAYFA ÇİZİMLERİ (Projeye özel) =====

// Küçük ortak başlık/altlık yardımcıları:
static void header(const __FlashStringHelper* title, bool edit) {
  if (s_toastActive) return;
  s_disp.clearDisplay();
  s_disp.setTextColor(SSD1306_WHITE);
  s_disp.setTextSize(2);
  s_disp.setCursor(0, 0);
  s_disp.print(title);
  if (edit) { s_disp.setCursor(100, 0); s_disp.print("E"); } // Edit göstergesi
}

static void footer(const __FlashStringHelper* hint) {
  if (s_toastActive) return;
  s_disp.setTextSize(1);
  s_disp.setCursor(0, 56);
  s_disp.print(hint);
  s_disp.display();
}

// --- Sayfa-1: Setpoint (0..100) ---
void displayDrawPage1_Setpoint(int setpoint, bool edit) {
  if (s_toastActive) return;
  header(F("SETPT"), edit);

  // Değeri büyük puntoda göster
  s_disp.setTextSize(3);
  s_disp.setCursor(10, 24);
  s_disp.printf("%3d", setpoint);

  // Basit bar (0..100 -> 0..110 px)
  int barW = map(setpoint, 0, 100, 0, 110);
  s_disp.drawRect(9, 22, 112, 18, SSD1306_WHITE);
  s_disp.fillRect(10, 23, barW, 16, SSD1306_WHITE);

  footer(F("Push:Edit  Enc:+/-  OK:Save  Back:Cancel"));
}

// --- Sayfa-2: Volume (0..10) ---
void displayDrawPage2_Volume(int volume, bool edit) {
  if (s_toastActive) return;
  header(F("VOL"), edit);

  s_disp.setTextSize(3);
  s_disp.setCursor(10, 24);
  s_disp.printf("%2d", volume);

  // 11 kademeli bar
  int cells = 11;
  int filled = constrain(volume, 0, 10);
  for (int i = 0; i < cells; ++i) {
    int x = 8 + i * 10;
    s_disp.drawRect(x, 22, 8, 18, SSD1306_WHITE);
    if (i <= filled) s_disp.fillRect(x+1, 23, 7, 16, SSD1306_WHITE);
  }

  footer(F("Push:Edit  Enc:+/-  OK:Save  Back:Cancel"));
}

// --- Sayfa-3: Mode {AUTO, MAN, OFF} ---
void displayDrawPage3_Mode(const char* mode, int idx, int count, bool edit) {
  if (s_toastActive) return;
  header(F("MODE"), edit);

  // Mevcut modu büyüt
  s_disp.setTextSize(2);
  int16_t x1,y1; uint16_t w,h;
  s_disp.getTextBounds(mode, 0, 0, &x1, &y1, &w, &h);
  int x = (OLED_W - (int)w)/2; if (x < 0) x = 0;
  s_disp.setCursor(x, 28);
  s_disp.print(mode);

  // Küçük index göstergesi (örn. 1/3)
  s_disp.setTextSize(1);
  s_disp.setCursor(54, 48);
  s_disp.printf("%d/%d", idx+1, count);

  footer(F("Push:Edit  Enc:Cycle  OK:Save  Back:Cancel"));
}