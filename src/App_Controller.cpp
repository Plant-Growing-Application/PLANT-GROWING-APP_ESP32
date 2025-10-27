#include <Arduino.h>
#include "Pins.h"
#include "Controls.h"
#include "Display.h"
#include "App_Controller.h"

// ------------------------------------------------------------
// Sayfa işleyici şablonu
// ------------------------------------------------------------
struct PageHandler {
  void (*onEnter)();    // sayfaya girildiğinde 1 kez
  void (*onDraw)();     // sayfa çizimi (özel)
  void (*onConfirm)();  // CONFIRM
  void (*onBack)();     // BACK
  void (*onPush)();     // Encoder PUSH
};

// ------------------------------------------------------------
// Uygulama durumu
// ------------------------------------------------------------
static int  s_page = 0;     // aktif sayfa: 0..(kAppPageCount-1)
static bool s_edit = false; // edit modu açık mı?

// --- Sayfa 1: Setpoint (0..100) ---
static int s_p1_setpoint = 50; // kalıcı
static int s_p1_pending  = 50; // edit sırasında geçici

// --- Sayfa 2: Volume (0..10) ---
static int s_p2_volume  = 5;
static int s_p2_pending = 5;

// --- Sayfa 3: Mode listesi ---
static const char* kModes[] = { "AUTO", "MAN", "OFF" };
static const int   kModeCount = sizeof(kModes)/sizeof(kModes[0]);
static int s_p3_mode    = 0; // 0..2
static int s_p3_pending = 0;

// ------------------------------------------------------------
// Yardımcılar
// ------------------------------------------------------------
static void drawCurrent(); // ileri deklarasyon

static void enterEdit(bool on) {
  if (on == s_edit) return;
  s_edit = on;
  displayShowToast(on ? "EDIT" : "VIEW", 600);
  drawCurrent();
}

// Edit modundaysak mevcut sayfanın değerini encoder ile değiştir
static void adjustCurrentPage(int det) {
  if (det == 0) return;

  switch (s_page) {
    case 0: // Setpoint 0..100
      s_p1_pending = constrain(s_p1_pending + det, 0, 100);
      break;

    case 1: // Volume 0..10
      s_p2_pending = constrain(s_p2_pending + det, 0, 10);
      break;

    case 2: // Mode 0..(N-1) sarmal
      s_p3_pending = (s_p3_pending + (det > 0 ? +1 : -1) + kModeCount) % kModeCount;
      break;
  }

  drawCurrent();
}

// Aktif sayfayı çiz
static void drawCurrent() {
  switch (s_page) {
    case 0: displayDrawPage1_Setpoint(s_p1_pending, s_edit); break;
    case 1: displayDrawPage2_Volume  (s_p2_pending, s_edit); break;
    case 2: displayDrawPage3_Mode    (kModes[s_p3_pending], s_p3_pending, kModeCount, s_edit); break;
    default: displayDrawPage(s_page); break;
  }
}

// ------------------------------------------------------------
// Sayfa 1 handler'ları
// ------------------------------------------------------------
static void p1_enter()  { s_edit = false; s_p1_pending = s_p1_setpoint; }
static void p1_draw()   { displayDrawPage1_Setpoint(s_p1_pending, s_edit); }
static void p1_confirm(){
  if (s_edit) { s_p1_setpoint = s_p1_pending; displayShowToast("Saved", 600); enterEdit(false); }
  else        { displayShowToast("ONAY", 600); }
}
static void p1_back(){
  if (s_edit) { s_p1_pending = s_p1_setpoint; displayShowToast("Cancel", 600); enterEdit(false); }
  else        { displayShowToast("GERI", 600); }
}
static void p1_push()   { enterEdit(!s_edit); }

// ------------------------------------------------------------
// Sayfa 2 handler'ları
// ------------------------------------------------------------
static void p2_enter()  { s_edit = false; s_p2_pending = s_p2_volume; }
static void p2_draw()   { displayDrawPage2_Volume(s_p2_pending, s_edit); }
static void p2_confirm(){
  if (s_edit) { s_p2_volume = s_p2_pending; displayShowToast("Saved", 600); enterEdit(false); }
  else        { displayShowToast("ONAY", 600); }
}
static void p2_back(){
  if (s_edit) { s_p2_pending = s_p2_volume; displayShowToast("Cancel", 600); enterEdit(false); }
  else        { displayShowToast("GERI", 600); }
}
static void p2_push()   { enterEdit(!s_edit); }

// ------------------------------------------------------------
// Sayfa 3 handler'ları
// ------------------------------------------------------------
static void p3_enter()  { s_edit = false; s_p3_pending = s_p3_mode; }
static void p3_draw()   { displayDrawPage3_Mode(kModes[s_p3_pending], s_p3_pending, kModeCount, s_edit); }
static void p3_confirm(){
  if (s_edit) { s_p3_mode = s_p3_pending; displayShowToast("Saved", 600); enterEdit(false); }
  else        { displayShowToast("ONAY", 600); }
}
static void p3_back(){
  if (s_edit) { s_p3_pending = s_p3_mode; displayShowToast("Cancel", 600); enterEdit(false); }
  else        { displayShowToast("GERI", 600); }
}
static void p3_push()   { enterEdit(!s_edit); }

// ------------------------------------------------------------
// Sayfa tablosu (PageHandler dizisi)
// ------------------------------------------------------------
static PageHandler kPages[] = {
  { p1_enter, p1_draw, p1_confirm, p1_back, p1_push }, // PAGE 1
  { p2_enter, p2_draw, p2_confirm, p2_back, p2_push }, // PAGE 2
  { p3_enter, p3_draw, p3_confirm, p3_back, p3_push }, // PAGE 3
};

static_assert(sizeof(kPages)/sizeof(kPages[0]) == kAppPageCount,
              "kPages boyutu kAppPageCount ile eslesmeli!");

// ------------------------------------------------------------
// APP yaşam döngüsü
// ------------------------------------------------------------
void appBegin(){
  Serial.begin(115200);

  controlsBegin();

  if (!displayBegin()){
    Serial.println(F("[ERR] OLED bulunamadi; goruntusuz devam edilecek."));
    // İstersen burada return; ile tamamen durdurabilirsin.
  }

  if (kPages[s_page].onEnter) kPages[s_page].onEnter();
  drawCurrent();
}

void appLoop(){
  // --- Encoder: edit'te değer değiştir; değilse sayfa değiştir ---
  const int det = controlsReadEncoderDetent();
  if (det != 0) {
    if (s_edit) {
      adjustCurrentPage(det); // (içerde drawCurrent() çağrılır)
    } else {
      const int prev = s_page;
      s_page = (s_page + (det > 0 ? +1 : -1) + kAppPageCount) % kAppPageCount;
      if (s_page != prev && kPages[s_page].onEnter) kPages[s_page].onEnter();
      drawCurrent();
    }
  }

  // --- Butonlar ---
  if (controlsButtonFell(Button::EncoderPush)) {
    if (kPages[s_page].onPush) kPages[s_page].onPush();
    // enterEdit() zaten çizdiriyor
  }

  if (controlsButtonFell(Button::Confirm)) {
    if (kPages[s_page].onConfirm) kPages[s_page].onConfirm();
    // Non-blocking toast: displayTick() süresi dolunca menüye geri döndürecek
  }

  if (controlsButtonFell(Button::Back)) {
    if (kPages[s_page].onBack) kPages[s_page].onBack();
  }

  // --- Non-blocking toast güncelle ---
  displayTick(s_page);
}
