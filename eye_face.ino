// ============================================================================
//  eye_face.ino  -  Mini-OS cho ESP32 + ST7789V 2.8" cam ung (man NGANG)
//  Giao dien Frutiger Aero: nen troi gradient, bong bong, nut kinh bong.
//  Menu vuot trang + cac app: Mat AI (gop het), Matrix, Do sang, Thong tin, Ngu
//
//  Thu vien: LovyanGFX (lovyan03).  Cac file cung thu muc "eye_face":
//      eye_face.ino  eye_engine.h  eye_bin.h  mieng_bin.h   (eye_engine.h GIU NGUYEN)
//
//  Thao tac:  vuot trai/phai = doi trang menu | cham icon = mo app
//             goc duoi-phai = nut nha (thoat ve menu)
//             Trong app Mat AI: goc duoi-trai = nut cai dat (cam xuc, noi, toc do)
//             Trong Matrix: giu 3 giay o bat ky dau de thoat
// ============================================================================
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <esp_heap_caps.h>

#define EYE_BG           0xFFF0
#define EYE_KEY          0xFFFF
#ifndef BC_LIFT
#define BC_LIFT 20
#endif

#define MOUTH_F_BC       0
#define MOUTH_F_BIG      1
#define MOUTH_F_MID      2
#define MOUTH_F_SMALL    3
#define MOUTH_F_DEFAULT  4
#define MOUTH_F_SAD      5

#include "eye_bin.h"
#include "mieng_bin.h"
#include "eye_engine.h"

#define LAYER_L_TRANG      9
#define LAYER_L_DO         7
#define LAYER_L_DEN        6
#define LAYER_L_LONGTRANG  2
#define LAYER_L_LMAY       0
#define LAYER_R_TRANG      8
#define LAYER_R_DO         4
#define LAYER_R_DEN        5
#define LAYER_R_LONGTRANG  3
#define LAYER_R_LMAY       1
#define BC_MIRROR_L        true
#define BC_MIRROR_R        false

#define TARGET_FPS       60
static const int  TFT_ROTATION  = 1;
static const bool TFT_INVERT    = false;
static const bool TFT_RGB_ORDER = false;
static const uint32_t SPI_MHZ   = 40;
static const int  EYE_GAP       = 20;
static const int  EYE_Y_OFFSET  = 0;
static const int  MOUTH_Y_OFFSET = 0;
static const int  TOUCH_ROT     = 0;

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789  _panel;
  lgfx::Bus_SPI       _bus;
  lgfx::Light_PWM     _light;
  lgfx::Touch_XPT2046 _touch;
public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host    = VSPI_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = SPI_MHZ * 1000000;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 18;
      cfg.pin_mosi = 23;
      cfg.pin_miso = 19;
      cfg.pin_dc   = 2;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs   = 15;
      cfg.pin_rst  = 4;
      cfg.pin_busy = -1;
      cfg.panel_width  = 240;
      cfg.panel_height = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable   = false;
      cfg.invert     = TFT_INVERT;
      cfg.rgb_order  = TFT_RGB_ORDER;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel.config(cfg);
    }
    {
      auto cfg = _light.config();
      cfg.pin_bl      = 5;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
    {
      auto cfg = _touch.config();
      cfg.x_min = 300;   cfg.x_max = 3900;
      cfg.y_min = 200;   cfg.y_max = 3700;
      cfg.pin_int   = 27;
      cfg.bus_shared = false;
      cfg.offset_rotation = (TOUCH_ROT & 3) ^ 4;
      cfg.spi_host = HSPI_HOST;
      cfg.freq     = 1000000;
      cfg.pin_sclk = 14;
      cfg.pin_mosi = 13;
      cfg.pin_miso = 26;
      cfg.pin_cs   = 33;
      _touch.config(cfg);
      _panel.setTouch(&_touch);
    }
    setPanel(&_panel);
  }
};

static LGFX lcd;

#define RGB565(r,g,b) ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3)))
static const uint16_t C_BG   = 0x0000;
static const uint16_t C_TEXT = 0xFFFF;
static const uint16_t C_DARK = RGB565(15, 70, 140);    // chu xanh dam tren nen sang

// ============================ GIAO DIEN FRUTIGER AERO =======================
// tron 2 mau RGB565, t = 0..255 (0 = a, 255 = b)
static uint16_t mix(uint16_t a, uint16_t b, int t) {
  int ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31;
  int br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
  return (uint16_t)(((ar + (((br - ar) * t) >> 8)) << 11) | ((ag + (((bg - ag) * t) >> 8)) << 5) | (ab + (((bb - ab) * t) >> 8)));
}

// do thut vao 2 ben cua hang j trong hinh chu nhat bo goc (ban kinh r, cao h)
static int cornerInset(int j, int h, int r) {
  int d = -1;
  if (j < r) d = r - j;
  else if (j >= h - r) d = j - (h - r) + 1;
  if (d < 0) return 0;
  return r - (int)sqrtf((float)(r * r - d * d));
}

// nut KINH BONG: do bong + gradient doc + vet sang o nua tren + vien sang
static void glossyTile(int x, int y, int w, int h, int r, uint16_t top, uint16_t bot) {
  lcd.fillRoundRect(x + 2, y + 3, w, h, r, RGB565(25, 85, 150));            // bong do
  int gh = h * 11 / 20;
  for (int j = 0; j < h; j++) {
    int in = cornerInset(j, h, r);
    uint16_t c = mix(top, bot, j * 255 / (h - 1));
    lcd.drawFastHLine(x + in, y + j, w - 2 * in, c);
    if (j >= 2 && j <= gh) {                                               // vet kinh
      int a = 105 - 75 * j / gh;
      lcd.drawFastHLine(x + in + 3, y + j, w - 2 * (in + 3), mix(c, C_TEXT, a));
    }
  }
  lcd.drawRoundRect(x, y, w, h, r, mix(top, C_TEXT, 170));
}

// chu trang co bong, dat giua (x,y)
static void label(const char* s, int x, int y) {
  lcd.setFont(&fonts::Font2);
  lcd.setTextDatum(lgfx::middle_center);
  lcd.setTextColor(RGB565(15, 60, 120)); lcd.drawString(s, x + 1, y + 1);
  lcd.setTextColor(C_TEXT);              lcd.drawString(s, x, y);
}

// nen troi gradient + bong bong
static void skyBg() {
  const uint16_t top = RGB565(20, 120, 230), mid = RGB565(100, 205, 255), bot = RGB565(150, 238, 185);
  const int H = lcd.height(), W = lcd.width();
  for (int y = 0; y < H; y += 4) {
    int t = y * 255 / H;
    lcd.fillRect(0, y, W, 4, t < 128 ? mix(top, mid, t * 2) : mix(mid, bot, (t - 128) * 2));
  }
  static const int16_t bx[6] = { 40, 285, 150, 300, 20, 235 };
  static const int16_t by[6] = { 190, 70, 215, 185, 105, 25 };
  static const int16_t br[6] = { 14, 10, 7, 18, 8, 6 };
  for (int i = 0; i < 6; i++) {
    lcd.drawCircle(bx[i], by[i], br[i], RGB565(230, 250, 255));
    lcd.drawCircle(bx[i], by[i], br[i] - 1, RGB565(170, 225, 250));
    lcd.fillCircle(bx[i] - br[i] / 3, by[i] - br[i] / 3, br[i] / 4 + 1, C_TEXT);
  }
}

// thanh tieu de dang vien thuoc
static void titlePill(const char* s) {
  int W = lcd.width();
  glossyTile(W / 2 - 70, 4, 140, 26, 13, RGB565(255, 255, 255), RGB565(170, 215, 245));
  lcd.setFont(&fonts::Font2);
  lcd.setTextDatum(lgfx::middle_center);
  lcd.setTextColor(C_DARK);
  lcd.drawString(s, W / 2, 17);
}

// ============================ CAM UNG / CU CHI ==============================
struct TouchState {
  bool down, pressEdge, releaseEdge;
  int  x, y, sx, sy;
} tp;
static bool swallow = false;         // bo qua lan nha tay tiep theo

static void pollTouch(uint32_t now) {
  static uint32_t last = 0;
  tp.pressEdge = tp.releaseEdge = false;
  if (now - last < 20) return;
  last = now;
  uint16_t x, y;
  bool d = lcd.getTouch(&x, &y);
  if (d) {
    if (!tp.down) { tp.sx = x; tp.sy = y; tp.pressEdge = true; }
    tp.x = x; tp.y = y;
  } else if (tp.down) tp.releaseEdge = true;
  tp.down = d;
}

// ================================ APP ======================================
enum App : uint8_t { A_MENU, A_EYE, A_MATRIX, A_BRIGHT, A_INFO, A_SLEEP };
static App app = A_MENU;
static int  brightness = 255;
static bool inHome(int x, int y) { return x >= lcd.width() - 56 && y >= lcd.height() - 44; }
static bool inGear(int x, int y) { return x < 56 && y >= lcd.height() - 44; }

static void drawHome() {
  int x = lcd.width() - 28, y = lcd.height() - 22;
  glossyTile(x - 17, y - 17, 34, 34, 17, RGB565(120, 200, 255), RGB565(20, 100, 210));
  lcd.fillTriangle(x - 9, y + 1, x, y - 9, x + 9, y + 1, C_TEXT);
  lcd.fillRect(x - 6, y + 1, 12, 8, C_TEXT);
}

static void menuDraw();
static void brightDraw();
static void infoDraw();
static bool eyeEnter();
static void eyeExit();
static void matrixEnter();

static void gotoApp(uint8_t a) {
  swallow = tp.down;
  app = (App)a;
  switch (a) {
    case A_MENU:   lcd.setBrightness(brightness); menuDraw(); break;
    case A_MATRIX: matrixEnter(); break;
    case A_BRIGHT: brightDraw(); break;
    case A_INFO:   infoDraw(); break;
    case A_SLEEP:  lcd.fillScreen(C_BG); lcd.setBrightness(0); break;
    default: break;
  }
}

// ================================ MENU =====================================
struct Item { const char* name; App app; uint16_t top, bot; };
static const Item kItems[] = {
  { "Mat AI",    A_EYE,    RGB565( 90,180,255), RGB565( 10, 90,200) },
  { "Matrix",    A_MATRIX, RGB565(120,230,120), RGB565( 10,140, 45) },
  { "Do sang",   A_BRIGHT, RGB565(255,225,100), RGB565(240,140,  0) },
  { "Thong tin", A_INFO,   RGB565(200,160,255), RGB565(100, 60,200) },
  { "Ngu",       A_SLEEP,  RGB565(140,210,235), RGB565( 40,100,170) },
};
static const int N_ITEMS = sizeof(kItems) / sizeof(kItems[0]);
static const int PER_PAGE = 4;
static const int N_PAGES = (N_ITEMS + PER_PAGE - 1) / PER_PAGE;
static int menuPage = 0;

static void cellRect(int slot, int& x, int& y, int& w, int& h) {
  w = 144; h = 88;
  x = 12 + (slot & 1) * 152;
  y = 36 + (slot >> 1) * 96;
}

static void drawIcon(int id, int cx, int cy, uint16_t bot) {
  switch (id) {
    case 0:  // mat
      for (int s = -1; s <= 1; s += 2) {
        lcd.fillCircle(cx + s * 17, cy, 14, C_TEXT);
        lcd.fillCircle(cx + s * 17 + 2, cy + 1, 7, 0x0000);
        lcd.fillCircle(cx + s * 17 + 4, cy - 2, 2, C_TEXT);
      }
      break;
    case 1:  // matrix: cac cot chu roi
      for (int c = 0; c < 5; c++)
        for (int r = 0; r < 4; r++)
          lcd.fillRect(cx - 26 + c * 11, cy - 20 + r * 10 + (c & 1) * 6, 7, 7,
                       r == 3 ? C_TEXT : mix(RGB565(0, 70, 20), RGB565(160, 255, 160), r * 80));
      break;
    case 2:  // mat troi
      lcd.fillCircle(cx, cy, 10, C_TEXT);
      for (int a = 0; a < 8; a++) {
        float r = a * 0.7854f;
        lcd.drawLine(cx + cosf(r) * 15, cy + sinf(r) * 15, cx + cosf(r) * 22, cy + sinf(r) * 22, C_TEXT);
      }
      break;
    case 3:  // info
      lcd.fillCircle(cx, cy, 20, C_TEXT);
      lcd.setFont(&fonts::Font4); lcd.setTextDatum(lgfx::middle_center);
      lcd.setTextColor(bot); lcd.drawString("i", cx, cy + 1);
      break;
    case 4:  // ngu: Zzz
      lcd.setFont(&fonts::Font4); lcd.setTextDatum(lgfx::middle_center);
      lcd.setTextColor(RGB565(15, 60, 120)); lcd.drawString("Zzz", cx + 1, cy + 1);
      lcd.setTextColor(C_TEXT);              lcd.drawString("Zzz", cx, cy);
      break;
  }
}

static void menuDraw() {
  skyBg();
  titlePill("MENU");
  for (int s = 0; s < PER_PAGE; s++) {
    int i = menuPage * PER_PAGE + s;
    if (i >= N_ITEMS) break;
    int x, y, w, h; cellRect(s, x, y, w, h);
    glossyTile(x, y, w, h, 16, kItems[i].top, kItems[i].bot);
    drawIcon(i, x + w / 2, y + 34, kItems[i].bot);
    label(kItems[i].name, x + w / 2, y + h - 14);
  }
  for (int p = 0; p < N_PAGES; p++) {
    int dx = lcd.width() / 2 + (int)((p - (N_PAGES - 1) * 0.5f) * 16), dy = lcd.height() - 8;
    if (p == menuPage) { lcd.fillCircle(dx, dy, 5, C_TEXT); lcd.drawCircle(dx, dy, 5, C_DARK); }
    else                 lcd.fillCircle(dx, dy, 4, RGB565(150, 205, 240));
  }
}

static void menuLoop() {
  if (!tp.releaseEdge) return;
  if (swallow) { swallow = false; return; }
  int dx = tp.x - tp.sx, dy = tp.y - tp.sy;
  if (abs(dx) > 40 && abs(dx) > abs(dy)) {
    int np = menuPage + (dx < 0 ? 1 : -1);
    if (np >= 0 && np < N_PAGES) { menuPage = np; menuDraw(); }
    return;
  }
  if (abs(dx) > 18 || abs(dy) > 18) return;
  for (int s = 0; s < PER_PAGE; s++) {
    int i = menuPage * PER_PAGE + s;
    if (i >= N_ITEMS) break;
    int x, y, w, h; cellRect(s, x, y, w, h);
    if (tp.sx >= x && tp.sx < x + w && tp.sy >= y && tp.sy < y + h) {
      const Item& it = kItems[i];
      if (it.app == A_EYE) { if (eyeEnter()) app = A_EYE; else menuDraw(); }
      else gotoApp(it.app);
      return;
    }
  }
}

// ============================= APP DO SANG ==================================
static const int SL_X0 = 30, SL_X1 = 290, SL_Y = 110;
static void brightBar() {
  int fx = SL_X0 + (brightness - 10) * (SL_X1 - SL_X0) / 245;
  lcd.fillRoundRect(SL_X0, SL_Y, SL_X1 - SL_X0, 16, 8, RGB565(215, 238, 252));
  lcd.drawRoundRect(SL_X0, SL_Y, SL_X1 - SL_X0, 16, 8, RGB565(90, 160, 220));
  lcd.fillRoundRect(SL_X0 + 2, SL_Y + 2, fx - SL_X0, 12, 6, mix(RGB565(255, 225, 100), RGB565(240, 140, 0), 128));
  lcd.fillCircle(fx, SL_Y + 8, 13, C_TEXT);
  lcd.drawCircle(fx, SL_Y + 8, 13, RGB565(90, 160, 220));
  lcd.fillCircle(fx - 3, SL_Y + 4, 4, RGB565(225, 245, 255));
}
static void brightDraw() {
  skyBg();
  titlePill("DO SANG");
  brightBar();
  drawHome();
}
static void brightLoop() {
  if (tp.pressEdge && inHome(tp.sx, tp.sy)) { gotoApp(A_MENU); return; }
  if (tp.down && !inHome(tp.x, tp.y) && tp.y > 60 && tp.y < 170) {
    int b = 10 + (tp.x - SL_X0) * 245 / (SL_X1 - SL_X0);
    b = b < 10 ? 10 : (b > 255 ? 255 : b);
    if (b != brightness) { brightness = b; lcd.setBrightness(b); brightBar(); }
  }
}

// ============================= APP THONG TIN ================================
static const uint16_t C_PANEL = RGB565(228, 244, 253);
static void infoDraw() {
  skyBg();
  titlePill("THONG TIN");
  lcd.fillRoundRect(8, 38, 304, 152, 14, C_PANEL);
  lcd.drawRoundRect(8, 38, 304, 152, 14, RGB565(90, 160, 220));
  lcd.setFont(&fonts::Font2);
  lcd.setTextDatum(lgfx::top_left);
  lcd.setTextColor(C_DARK, C_PANEL);
  drawHome();
}
static void infoLoop() {
  if (tp.pressEdge && inHome(tp.sx, tp.sy)) { gotoApp(A_MENU); return; }
  static uint32_t t = 0;
  if (millis() - t < 1000) return;
  t = millis();
  char s[48];
  lcd.setFont(&fonts::Font2); lcd.setTextDatum(lgfx::top_left);
  lcd.setTextColor(C_DARK, C_PANEL); lcd.setTextPadding(250);
  snprintf(s, sizeof s, "RAM trong: %u B", (unsigned)ESP.getFreeHeap());              lcd.drawString(s, 24, 50);
  snprintf(s, sizeof s, "RAM thap nhat: %u B", (unsigned)ESP.getMinFreeHeap());        lcd.drawString(s, 24, 80);
  snprintf(s, sizeof s, "Khoi lon nhat: %u B", (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)); lcd.drawString(s, 24, 110);
  snprintf(s, sizeof s, "CPU: %u MHz", (unsigned)ESP.getCpuFreqMHz());                 lcd.drawString(s, 24, 140);
  snprintf(s, sizeof s, "Chay: %lu s", (unsigned long)(millis() / 1000));              lcd.drawString(s, 24, 164);
  lcd.setTextPadding(0);
}

// ============================== APP MATRIX ==================================
// Khong dung framebuffer: moi cot chi luu vai byte; o chu duoc sinh tu "seed" nen
// cung 1 o luon ra cung 1 ky tu -> khong can luu luoi chu.
static const int MX_COLS = 40, MX_ROWS = 24, MX_CW = 8, MX_CH = 10;
static int16_t  mxHead[MX_COLS];
static uint8_t  mxLen[MX_COLS], mxSpd[MX_COLS];
static float    mxAcc[MX_COLS];
static uint16_t mxSeed[MX_COLS];
static uint32_t mxLast, mxHold;
static const uint16_t mxC[5] = { RGB565(215,255,215), RGB565(0,255,70), RGB565(0,190,50), RGB565(0,120,30), RGB565(0,65,18) };

static void mxReset(int c, bool first) {
  uint32_t r = esp_random();
  mxHead[c] = -(int)((r & 0xFFFF) % (first ? MX_ROWS : 8));
  mxLen[c]  = 12 + ((r >> 16) % 12);
  mxSpd[c]  = 6 + ((r >> 20) % 20);
  mxSeed[c] = (uint16_t)esp_random();
  mxAcc[c]  = 0.f;
}

static void mxCell(int c, int row, uint16_t color) {      // color 0 = xoa
  if (row < 0 || row >= MX_ROWS) return;
  int x = c * MX_CW, y = row * MX_CH;
  if (!color) { lcd.fillRect(x, y, MX_CW, MX_CH, 0); return; }
  char s[2] = { (char)(33 + ((c * 131u + row * 31u + mxSeed[c]) % 94)), 0 };
  lcd.setTextColor(color, 0);
  lcd.drawString(s, x + 1, y + 1);
}

static void matrixEnter() {
  lcd.fillScreen(0);
  lcd.setFont(&fonts::Font0);
  lcd.setTextSize(1);
  lcd.setTextDatum(lgfx::top_left);
  for (int c = 0; c < MX_COLS; c++) mxReset(c, true);
  mxLast = millis();
  mxHold = mxLast;
}

static void matrixLoop() {
  uint32_t now = millis();
  if (!tp.down) mxHold = now;                                   // nha tay -> dem lai tu dau
  else if (now - mxHold >= 3000) { gotoApp(A_MENU); return; }   // giu 3 giay o bat ky dau -> thoat
  float dt = (now - mxLast) * 1e-3f; mxLast = now;
  if (dt > 0.06f) dt = 0.06f;
  lcd.startWrite();
  for (int c = 0; c < MX_COLS; c++) {
    mxAcc[c] += mxSpd[c] * dt;
    while (mxAcc[c] >= 1.f) {
      mxAcc[c] -= 1.f;
      int h = ++mxHead[c];
      mxCell(c, h,      mxC[0]);
      mxCell(c, h - 1,  mxC[1]);
      mxCell(c, h - 3,  mxC[2]);
      mxCell(c, h - 6,  mxC[3]);
      mxCell(c, h - 10, mxC[4]);
      mxCell(c, h - mxLen[c], 0);
      if (h - mxLen[c] >= MX_ROWS) { mxReset(c, false); break; }
    }
  }
  lcd.endWrite();
}

// ============================== APP MAT AI ==================================
static EyeSet   gL, gR;
static EyeBrain brain;
static uint16_t *bufL = nullptr, *bufR = nullptr, *bufM = nullptr;
static EyeRender prevL, prevR;
static int  prevMouth = -1;
static bool firstFrame = true, fixedMood = false, brainSeeded = false, cfgOpen = false;
static int  xL, xR, yEye, xM, yMouth;

// cai dat cua app Mat (giu nguyen khi thoat/vao lai)
static int  cfgMood  = -1;       // 0..4 co dinh, -1 = tu dong
static bool talkOn   = false;    // mieng noi lien tuc
static int  speedLvl = 2;        // 0..4 (toc do mieng)
static const float kPeriod[5] = { 2.8f, 2.0f, 1.6f, 1.1f, 0.8f };   // giay / 1 vong mieng (nho = nhanh)
static const char* kMoodNames[6] = { "Binh thuong", "Gian", "Buon", "Vui", "Buon ngu", "Tu dong" };
static const uint16_t kMoodTop[6] = { RGB565(110,190,255), RGB565(255,130,120), RGB565(170,190,230), RGB565(255,230,110), RGB565(200,160,255), RGB565(130,235,140) };
static const uint16_t kMoodBot[6] = { RGB565( 20,100,210), RGB565(200, 30, 30), RGB565( 70, 90,170), RGB565(240,150, 10), RGB565(110, 70,200), RGB565( 20,150, 60) };

static inline const uint16_t* eyeLayer(int idx) { return eye_bin_words + (size_t)idx * (EYE_W * EYE_H); }
static inline const uint16_t* mouthData(int idx) { return mieng_bin_words + (size_t)idx * (EYE_W * EYE_H); }

static bool buildEye(EyeSet& e, int trang, int doo, int den, int longtrang, int lmay) {
  return eyeBuild(e, eyeLayer(trang), eyeLayer(doo), eyeLayer(den), eyeLayer(longtrang), eyeLayer(lmay), true);
}
static void freeEye(EyeSet& e) {
  free(e.base); free(e.iris.px); free(e.pupil.px); free(e.glint.px);
  free(e.lid.px); free(e.bc.px); free(e.lidLow);
  memset(&e, 0, sizeof(e));
}
static void freeBufs() {
  heap_caps_free(bufL); heap_caps_free(bufR); heap_caps_free(bufM);
  bufL = bufR = bufM = nullptr;
}

static inline void pushImg(int x, int y, uint16_t* buf) {
  lcd.pushImageDMA(x, y, EYE_W, EYE_H, (const lgfx::swap565_t*)buf);
}
static inline void pushEye(int x, int y, uint16_t* buf) {
  lcd.pushImageDMA(x, y - EYE_PAD_TOP, EYE_W, EYE_H + EYE_PAD_TOP, (const lgfx::swap565_t*)buf);
}

static void drawGear() {                           // nut cai dat o goc duoi-trai
  int x = 28, y = lcd.height() - 22;
  glossyTile(x - 17, y - 17, 34, 34, 17, RGB565(170, 190, 215), RGB565(70, 90, 130));
  for (int a = 0; a < 8; a++) {
    float r = a * 0.7854f;
    lcd.drawLine(x + cosf(r) * 8, y + sinf(r) * 8, x + cosf(r) * 13, y + sinf(r) * 13, C_TEXT);
  }
  lcd.fillCircle(x, y, 7, C_TEXT);
  lcd.fillCircle(x, y, 3, RGB565(70, 90, 130));
}

static void applyMood() {
  fixedMood = cfgMood >= 0;
  if (fixedMood) { brain.setMood((EyeMood)cfgMood); brain.moodHold = 3600.f; }
  else           { brain.moodHold = 0.f; brain.moodT = 1.f; }
}
static void applyTalk() {
  brain.speakForever = talkOn;
  if (talkOn) brain.speak(1.f);
}

static bool eyeEnter() {
  lcd.fillScreen(EYE_BG);
  bool ok = buildEye(gL, LAYER_L_TRANG, LAYER_L_DO, LAYER_L_DEN, LAYER_L_LONGTRANG, LAYER_L_LMAY) &&
            buildEye(gR, LAYER_R_TRANG, LAYER_R_DO, LAYER_R_DEN, LAYER_R_LONGTRANG, LAYER_R_LMAY);
  if (ok) {
    const uint16_t* bc = mouthData(MOUTH_F_BC);
    ok = eyeSetBc(gL, bc, true, BC_MIRROR_L, -1);
    if (ok) {
      int off  = eyeLayerCenterX(gL.bc) - gL.irisCx;
      int sign = (BC_MIRROR_L == BC_MIRROR_R) ? 1 : -1;
      ok = eyeSetBc(gR, bc, true, BC_MIRROR_R, gR.irisCx + sign * off);
    }
  }
  if (ok) {
    bufL = (uint16_t*)heap_caps_malloc(EYE_W * (EYE_H + EYE_PAD_TOP) * 2, MALLOC_CAP_DMA);
    bufR = (uint16_t*)heap_caps_malloc(EYE_W * (EYE_H + EYE_PAD_TOP) * 2, MALLOC_CAP_DMA);
    bufM = (uint16_t*)heap_caps_malloc(EYE_W * EYE_H * 2, MALLOC_CAP_DMA);
    ok = bufL && bufR && bufM;
  }
  if (!ok) {
    freeEye(gL); freeEye(gR); freeBufs();
    lcd.setFont(&fonts::Font2); lcd.setTextColor(0xF800, EYE_BG); lcd.setTextDatum(lgfx::top_left);
    lcd.drawString("Het RAM / loi du lieu!", 10, 10);
    delay(1500);
    return false;
  }

  xL     = lcd.width() / 2 - EYE_W - EYE_GAP / 2;
  xR     = lcd.width() / 2 + (EYE_GAP - EYE_GAP / 2);
  yEye   = lcd.height() / 2 - 80 + EYE_Y_OFFSET;
  xM     = lcd.width() / 2 - EYE_W / 2;
  yMouth = yEye + 88 + MOUTH_Y_OFFSET;

  if (!brainSeeded) { brain.seed(esp_random()); brainSeeded = true; }
  brain.mouthPeriod = kPeriod[speedLvl];
  brain.releaseLook();
  applyMood();
  applyTalk();
  cfgOpen = false;
  firstFrame = true; prevMouth = -1;
  drawHome(); drawGear();
  return true;
}

static void eyeExit() {
  brain.speakForever = false; brain.speaking = false; brain.moodHold = 0.f;
  freeEye(gL); freeEye(gR); freeBufs();
}

// ---- man hinh cai dat cua app Mat: cam xuc + noi + toc do ----
static void cfgTile(int i, int& x, int& y) { x = 8 + (i % 3) * 104; y = 38 + (i / 3) * 56; }

static void cfgDraw() {
  skyBg();
  titlePill("CAI DAT MAT");
  int sel = cfgMood < 0 ? 5 : cfgMood;
  for (int i = 0; i < 6; i++) {
    int x, y; cfgTile(i, x, y);
    glossyTile(x, y, 96, 50, 12, kMoodTop[i], kMoodBot[i]);
    if (i == sel) { lcd.drawRoundRect(x - 1, y - 1, 98, 52, 13, C_TEXT); lcd.drawRoundRect(x - 2, y - 2, 100, 54, 14, C_TEXT); }
    label(kMoodNames[i], x + 48, y + 25);
  }
  if (talkOn) glossyTile(8, 154, 110, 40, 10, RGB565(130, 235, 140), RGB565(20, 150, 60));
  else        glossyTile(8, 154, 110, 40, 10, RGB565(200, 210, 222), RGB565(120, 135, 150));
  label(talkOn ? "Noi: BAT" : "Noi: TAT", 63, 174);
  glossyTile(124, 154, 40, 40, 10, RGB565(120, 200, 255), RGB565(20, 100, 210));
  glossyTile(236, 154, 40, 40, 10, RGB565(120, 200, 255), RGB565(20, 100, 210));
  label("-", 144, 174);
  label("+", 256, 174);
  char s[16]; snprintf(s, sizeof s, "Toc do %d", speedLvl + 1);
  label(s, 200, 174);
  glossyTile(lcd.width() - 56, lcd.height() - 42, 52, 36, 10, RGB565(120, 200, 255), RGB565(20, 100, 210));
  label("OK", lcd.width() - 30, lcd.height() - 24);
}

static void cfgClose() {
  cfgOpen = false;
  lcd.fillScreen(EYE_BG);
  firstFrame = true; prevMouth = -1;
  drawHome(); drawGear();
}

static bool hit(int x, int y, int w, int h) { return tp.sx >= x && tp.sx < x + w && tp.sy >= y && tp.sy < y + h; }

static void cfgLoop() {
  if (!tp.releaseEdge) return;
  if (swallow) { swallow = false; return; }
  if (hit(lcd.width() - 56, lcd.height() - 44, 56, 44)) { cfgClose(); return; }
  for (int i = 0; i < 6; i++) {
    int x, y; cfgTile(i, x, y);
    if (hit(x, y, 96, 50)) { cfgMood = i < 5 ? i : -1; applyMood(); cfgDraw(); return; }
  }
  if (hit(8, 154, 110, 40))   { talkOn = !talkOn; applyTalk(); cfgDraw(); return; }
  if (hit(124, 154, 40, 40))  { if (speedLvl > 0) speedLvl--; brain.mouthPeriod = kPeriod[speedLvl]; cfgDraw(); return; }
  if (hit(236, 154, 40, 40))  { if (speedLvl < 4) speedLvl++; brain.mouthPeriod = kPeriod[speedLvl]; cfgDraw(); return; }
}

static void eyeLoop() {
  if (cfgOpen) { cfgLoop(); delay(10); return; }

  static uint32_t lastUs = micros();
  const uint32_t frameUs = 1000000UL / TARGET_FPS;
  uint32_t t0 = micros();
  float dt = (t0 - lastUs) * 1e-6f;
  lastUs = t0;
  if (dt > 0.05f) dt = 0.05f;

  if (tp.pressEdge && inHome(tp.sx, tp.sy)) { eyeExit(); gotoApp(A_MENU); return; }
  if (tp.pressEdge && inGear(tp.sx, tp.sy)) { cfgOpen = true; swallow = tp.down; cfgDraw(); return; }
  if (tp.down && !inHome(tp.x, tp.y) && !inGear(tp.x, tp.y)) {
    brain.lookAt(((float)tp.x - lcd.width() * 0.5f) / (lcd.width() * 0.5f),
                 ((float)tp.y - (yEye + EYE_H * 0.5f)) / (lcd.height() * 0.35f));
    if (tp.pressEdge) brain.speak(2.0f);
    if (!fixedMood) { brain.setMood(MOOD_HAPPY); brain.moodHold = 1.2f; }
  } else if (tp.releaseEdge) {
    brain.releaseLook();
    if (!fixedMood) brain.moodT = 0.8f;
  }
  brain.update(dt);

  EyeRender rl, rr;
  brain.compose(gL, +1, rl);
  brain.compose(gR, -1, rr);
  int mf = brain.mouthFrame();
  bool dl = firstFrame || memcmp(&rl, &prevL, sizeof(rl)) != 0;
  bool dr = firstFrame || memcmp(&rr, &prevR, sizeof(rr)) != 0;
  bool dm = firstFrame || mf != prevMouth;
  firstFrame = false;

  if (dl || dr || dm) {
    lcd.startWrite();
    if (dl) { eyeRender(gL, rl, bufL); prevL = rl; pushEye(xL, yEye, bufL); }
    if (dr) { eyeRender(gR, rr, bufR); prevR = rr; lcd.waitDMA(); pushEye(xR, yEye, bufR); }
    if (dm) {
      mouthRender(mouthData(mf), true, bufM);
      lcd.waitDMA();
      pushImg(xM, yMouth, bufM);
      prevMouth = mf;
    }
    lcd.endWrite();
  }

  uint32_t used = micros() - t0;
  if (used < frameUs) {
    uint32_t rem = frameUs - used;
    if (rem > 1500) delay((rem - 500) / 1000);
    while (micros() - t0 < frameUs) { }
  }
}

// ============================== SETUP / LOOP ================================
void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.setRotation(TFT_ROTATION);
  lcd.setBrightness(brightness);
  gotoApp(A_MENU);
}

void loop() {
  pollTouch(millis());
  switch (app) {
    case A_MENU:   menuLoop();   break;
    case A_EYE:    eyeLoop();    return;
    case A_MATRIX: matrixLoop(); break;
    case A_BRIGHT: brightLoop(); break;
    case A_INFO:   infoLoop();   break;
    case A_SLEEP:  if (tp.pressEdge) { gotoApp(A_MENU); } break;
  }
  delay(10);
}
