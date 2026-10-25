// ============================================================================
//  eye_face.ino  -  Mini-OS cho ESP32 + ST7789V 2.8" cam ung (man NGANG)
//  2 THEME: Frutiger Aero + Sakura (hong-trang-tim, hoa anh dao). Cham thanh "MENU" de doi.
//  Menu vuot trang + cac app: Mat AI, Chat AI, Matrix, Do sang, Thong tin, Ngu, Anh GIF
//
//  Thu vien: LovyanGFX, ArduinoJson (v7), AnimatedGIF (bitbank2)
//  File cung thu muc "eye_face":
//      eye_face.ino  eye_engine.h  eye_bin.h  mieng_bin.h  nyan_bin.h  my_gif.h
//  my_gif.h:  const uint8_t my_gif[] PROGMEM = { 0x47, 0x49, 0x46, ... };
//
//  Thao tac:  vuot trai/phai = doi trang menu | cham icon = mo app | cham "MENU" = doi theme
//             goc duoi-phai = nut nha | Mat AI: goc duoi-trai = cai dat
//             Matrix: giu 3 giay de thoat | Chat AI: cham thanh duoi = go phim
//  Man hinh cho: 2 phut khong cham -> Nyan Cat, cham de thoat.
// ============================================================================
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <esp_heap_caps.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>
#include <AnimatedGIF.h>
#include "my_gif.h"

struct Msg { bool me = false, err = false; String t, wr; uint16_t w = 0; uint8_t lines = 1; };

// ============================ CAU HINH CHAT AI ==============================
// !!! DIEN WIFI VA API KEY CUA CAU VAO DAY (tren file nay tớ de trong) !!!
static const char* WIFI_SSID = "TEN_WIFI";
static const char* WIFI_PASS = "MAT_KHAU_WIFI";
static const char* API_URL   = "https://generativelanguage.googleapis.com/v1beta/openai/chat/completions";
static const char* API_KEY   = "DAN_API_KEY_VAO_DAY";
static const char* API_MODEL = "gemini-3.5-flash-lite";
#define WIFI_OFF_ON_EXIT 1
static uint8_t WIFI_MAC[6] = { 0xF6, 0x91, 0xE6, 0x7C, 0x19, 0x06 };
static const char* SYS_PROMPT =
  "Ban la Anble AI, tro ly ao hien thi tren man hinh nho cua ESP32, "
  "Tra loi bang tieng Viet khong dau hoac co dau ngan gon, toi da duoi 25 tu, "
  "tinh cach cute dang yeu phu hop voi frutiger aego, "
  "Khong dung markdown, khong emoji, khong giai thich dong dai. ";

#define EYE_BG           0xFFF0
#define EYE_KEY          0xFFFF
#ifndef BC_LIFT
#define BC_LIFT 20
#endif

// Khung trong mieng_bin.h:  0 mac dinh | 1 buon | 2 vui | 3..7 noi | 8 bc (long may)
#define MOUTH_F_DEFAULT  0
#define MOUTH_F_SAD      1
#define MOUTH_F_HAPPY    2
#define MOUTH_F_BC       8

#include "eye_bin.h"
#include "mieng_bin.h"
#include "nyan_bin.h"
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
#define BC_GAP             56      // px: khoang cach giua 2 bc (mep trong trai <-> mep trong phai) tren man hinh.
                                   // Doc lap voi mat. Tang = xa nhau, giam = gan nhau (toi da ~80).
                                   // LUU Y: ~26 la khoang cach cu -> so gan 26 se nhin khong doi.
#define BC_SHIFT_L         0       // px: dich RIENG bc mat trai  (+ sang phai, - sang trai)
#define BC_SHIFT_R         0       // px: dich RIENG bc mat phai  (+ sang phai, - sang trai)

#define TARGET_FPS       60
static const int  TFT_ROTATION  = 1;
static const bool TFT_INVERT    = false;
static const bool TFT_RGB_ORDER = false;
static const uint32_t SPI_MHZ   = 40;
static const int  EYE_GAP       = 20;
static const int  EYE_Y_OFFSET  = 30;
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

// ================================ THEME =====================================
struct Theme {
  uint16_t skyTop, skyMid, skyBot, shadow, lshadow, dark, panel, edge, accT, accB, mine, pillBot;
};
static const Theme kThemes[2] = {
  // 0 = Frutiger Aero
  { RGB565(20,120,230), RGB565(100,205,255), RGB565(150,238,185), RGB565(25,85,150), RGB565(15,60,120),
    RGB565(15,70,140), RGB565(228,244,253), RGB565(90,160,220), RGB565(120,200,255), RGB565(20,100,210),
    RGB565(0,132,255), RGB565(170,215,245) },
  // 1 = Yae (do hong - kem - tim)
  { RGB565(224,92,80), RGB565(244,140,120), RGB565(253,172,150), RGB565(150,58,52), RGB565(140,46,50),
    RGB565(160,56,54), RGB565(255,243,236), RGB565(246,188,172), RGB565(255,208,190), RGB565(226,102,94),
    RGB565(226,96,88), RGB565(252,220,206) },
};
static uint8_t theme = 1;                      // 0 = Frutiger, 1 = Sakura
#define TH       kThemes[theme]
#define C_DARK   (TH.dark)
#define C_PANEL  (TH.panel)
#define SHADOW   (TH.shadow)
#define LSHADOW  (TH.lshadow)
#define EDGE     (TH.edge)
#define ACC_T    (TH.accT)
#define ACC_B    (TH.accB)
#define MINE     (TH.mine)

// ============================ GIAO DIEN =====================================
// tron 2 mau RGB565, t = 0..255 (0 = a, 255 = b)
static uint16_t mix(uint16_t a, uint16_t b, int t) {
  int ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31;
  int br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
  return (uint16_t)(((ar + (((br - ar) * t) >> 8)) << 11) | ((ag + (((bg - ag) * t) >> 8)) << 5) | (ab + (((bb - ab) * t) >> 8)));
}

static int cornerInset(int j, int h, int r) {
  int d = -1;
  if (j < r) d = r - j;
  else if (j >= h - r) d = j - (h - r) + 1;
  if (d < 0) return 0;
  return r - (int)sqrtf((float)(r * r - d * d));
}

static void flower(int cx, int cy, int r, uint16_t c, uint16_t edge, uint16_t core);

// nut KINH BONG (Frutiger) / nut PHANG vien kem (Yae)
static void glossyTile(int x, int y, int w, int h, int r, uint16_t top, uint16_t bot) {
  lcd.fillRoundRect(x + 2, y + 3, w, h, r, SHADOW);
  if (theme == 1) {
    const uint16_t cream = RGB565(255, 240, 226);
    for (int j = 0; j < h; j++) {
      int in = cornerInset(j, h, r);
      lcd.drawFastHLine(x + in, y + j, w - 2 * in, mix(top, bot, j * 255 / (h - 1)));
    }
    lcd.drawRoundRect(x, y, w, h, r, cream);
    if (h > 20) lcd.drawRoundRect(x + 2, y + 2, w - 4, h - 4, r > 2 ? r - 2 : r, mix(top, cream, 130));
    if (w >= 100 && h >= 60) flower(x + w - 16, y + 16, 6, RGB565(255, 236, 228), RGB565(246, 184, 170), RGB565(218, 104, 92));
    return;
  }
  int gh = h * 11 / 20;
  for (int j = 0; j < h; j++) {
    int in = cornerInset(j, h, r);
    uint16_t c = mix(top, bot, j * 255 / (h - 1));
    lcd.drawFastHLine(x + in, y + j, w - 2 * in, c);
    if (j >= 2 && j <= gh) {
      int a = 105 - 75 * j / gh;
      lcd.drawFastHLine(x + in + 3, y + j, w - 2 * (in + 3), mix(c, C_TEXT, a));
    }
  }
  lcd.drawRoundRect(x, y, w, h, r, mix(top, C_TEXT, 170));
}

static void label(const char* s, int x, int y) {
  lcd.setFont(&fonts::Font2);
  lcd.setTextDatum(lgfx::middle_center);
  lcd.setTextColor(LSHADOW); lcd.drawString(s, x + 1, y + 1);
  lcd.setTextColor(C_TEXT);  lcd.drawString(s, x, y);
}

// ---------------- STYLE YAE: nen do-hong, luc giac, song, hoa anh dao ----------------
// canh hoa roi (dung/ngang), co vien
static void petal(int x, int y, int r, bool vert) {
  uint16_t c = RGB565(255, 232, 220), e = RGB565(246, 180, 166), hi = RGB565(255, 246, 240);
  int a = vert ? r * 2 / 3 : r, b = vert ? r : r * 2 / 3;
  lcd.fillEllipse(x, y, a + 1, b + 1, e);
  lcd.fillEllipse(x, y, a, b, c);
  lcd.fillCircle(x - a / 3, y - b / 3, r / 5 + 1, hi);
}

// hoa anh dao 5 canh (r = ban kinh canh)
static void flower(int cx, int cy, int r, uint16_t c, uint16_t edge, uint16_t core) {
  int pr = r * 7 / 10;
  for (int k = 0; k < 5; k++) {
    float a = k * 1.2566f - 1.5708f;
    lcd.fillCircle(cx + (int)(cosf(a) * r), cy + (int)(sinf(a) * r), pr + 1, edge);
  }
  for (int k = 0; k < 5; k++) {
    float a = k * 1.2566f - 1.5708f;
    lcd.fillCircle(cx + (int)(cosf(a) * r), cy + (int)(sinf(a) * r), pr, c);
  }
  lcd.fillCircle(cx, cy, r / 3 + 1, core);
}

static void yaeBg() {
  const int W = lcd.width(), H = lcd.height();
  const uint16_t c0 = RGB565(224, 92, 80), c1 = RGB565(253, 172, 150);
  for (int y = 0; y < H; y += 4)
    for (int x = 0; x < W; x += 16) {
      int t = x * 150 / W + y * 100 / H;
      lcd.fillRect(x, y, 16, 4, mix(c0, c1, t > 255 ? 255 : t));
    }
  // luoi luc giac mo
  const uint16_t hx = RGB565(236, 122, 104);
  for (int row = -1; row < 7; row++)
    for (int col = -1; col < 8; col++) {
      int cx = col * 45 + (row & 1) * 22, cy = row * 39 + 20;
      int vx[6], vy[6];
      for (int k = 0; k < 6; k++) {
        float a = 0.5236f + k * 1.0472f;
        vx[k] = cx + (int)(cosf(a) * 26); vy[k] = cy + (int)(sinf(a) * 26);
      }
      for (int k = 0; k < 6; k++) lcd.drawLine(vx[k], vy[k], vx[(k + 1) % 6], vy[(k + 1) % 6], hx);
    }
  // vong song
  const uint16_t wv = RGB565(246, 140, 122), wv2 = RGB565(255, 192, 170);
  for (int d = 0; d < 3; d++) {
    lcd.drawCircle(-10, H + 20, 96 + d, wv2);
    lcd.drawCircle(-10, H + 20, 128 + d, wv);
    lcd.drawCircle(W + 30, H + 40, 120 + d, wv);
    lcd.drawCircle(W - 20, -30, 70 + d, wv2);
  }
  // canh cay
  const uint16_t br = RGB565(150, 62, 52);
  for (int d = 0; d < 3; d++) {
    lcd.drawLine(-4, 8 + d, 70, 34 + d, br);
    lcd.drawLine(70, 34 + d, 150, 24 + d, br);
    lcd.drawLine(40, 22 + d, 56, 70 + d, br);
  }
  // cum hoa anh dao goc tren-trai
  static const int16_t fx[9] = { 22, 58, 94, 40, 128, 14, 78, 112, 150 };
  static const int16_t fy[9] = { 30, 14, 36, 62, 12, 78, 74, 64, 40 };
  static const int8_t  fr[9] = { 10, 12, 9, 13, 7, 8, 10, 7, 6 };
  for (int i = 0; i < 9; i++)
    flower(fx[i], fy[i], fr[i], RGB565(255, 218, 205), RGB565(242, 168, 154), RGB565(218, 104, 92));
  // hoa toi mau ben phai
  flower(W - 52, 74, 18, RGB565(190, 72, 66), RGB565(160, 52, 52), RGB565(125, 36, 40));
  // canh hoa roi
  static const int16_t px[10] = { 190, 235, 275, 300, 210, 20, 120, 255, 170, 300 };
  static const int16_t py[10] = { 20, 40, 14, 120, 110, 140, 200, 190, 160, 225 };
  for (int i = 0; i < 10; i++) petal(px[i], py[i], 4 + (i % 3) * 2, i & 1);
}

// vai canh hoa roi de len tren cac nut (goi sau khi ve nut)
static void yaeDeco() {
  static const int16_t px[6] = { 308, 160, 8, 160, 300, 30 };
  static const int16_t py[6] = { 36, 128, 128, 226, 128, 38 };
  for (int i = 0; i < 6; i++) petal(px[i], py[i], 4 + (i % 2) * 2, i & 1);
}

// nen gradient + bong bong (Frutiger) hoac hoa anh dao (Sakura)
static void skyBg() {
  if (theme == 1) { yaeBg(); return; }
  const Theme& t = TH;
  const int H = lcd.height(), W = lcd.width();
  for (int y = 0; y < H; y += 4) {
    int k = y * 255 / H;
    lcd.fillRect(0, y, W, 4, k < 128 ? mix(t.skyTop, t.skyMid, k * 2) : mix(t.skyMid, t.skyBot, (k - 128) * 2));
  }
  static const int16_t bx[6] = { 40, 285, 150, 300, 20, 235 };
  static const int16_t by[6] = { 190, 70, 215, 185, 105, 25 };
  static const int16_t br[6] = { 14, 10, 7, 18, 8, 6 };
  if (theme == 0) {
    for (int i = 0; i < 6; i++) {
      lcd.drawCircle(bx[i], by[i], br[i], RGB565(230, 250, 255));
      lcd.drawCircle(bx[i], by[i], br[i] - 1, RGB565(170, 225, 250));
      lcd.fillCircle(bx[i] - br[i] / 3, by[i] - br[i] / 3, br[i] / 4 + 1, C_TEXT);
    }
  } else {
    static const int16_t px[14] = { 18, 60, 105, 150, 205, 250, 300, 35, 130, 180, 270, 310, 85, 225 };
    static const int16_t py[14] = { 50, 190, 80, 215, 35, 120, 200, 130, 150, 100, 60, 150, 20, 175 };
    for (int i = 0; i < 14; i++) petal(px[i], py[i], 4 + (i % 4) * 2, i & 1);
  }
}

static void titlePill(const char* s) {
  int W = lcd.width();
  glossyTile(W / 2 - 70, 4, 140, 26, 13, RGB565(255, 255, 255), TH.pillBot);
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
static bool swallow = false;

static uint32_t lastTouchMs = 0;
static bool ignoreTouch = false;

static void pollTouch(uint32_t now) {
  static uint32_t last = 0;
  tp.pressEdge = tp.releaseEdge = false;
  if (now - last < 20) return;
  last = now;
  uint16_t x, y;
  bool d = lcd.getTouch(&x, &y);
  if (d) lastTouchMs = now;
  if (ignoreTouch) { if (!d) ignoreTouch = false; tp.down = false; return; }
  if (d) {
    if (!tp.down) { tp.sx = x; tp.sy = y; tp.pressEdge = true; }
    tp.x = x; tp.y = y;
  } else if (tp.down) tp.releaseEdge = true;
  tp.down = d;
}

// ================================ APP ======================================
enum App : uint8_t { A_MENU, A_EYE, A_CHAT, A_MATRIX, A_BRIGHT, A_INFO, A_SLEEP, A_GIF, A_DRAW };
static App app = A_MENU;
static int  brightness = 255;
static bool inHome(int x, int y) { return x >= lcd.width() - 56 && y >= lcd.height() - 44; }
static bool inGear(int x, int y) { return x < 56 && y >= lcd.height() - 44; }

static void drawHome() {
  int x = lcd.width() - 28, y = lcd.height() - 22;
  glossyTile(x - 17, y - 17, 34, 34, 17, ACC_T, ACC_B);
  lcd.fillTriangle(x - 9, y + 1, x, y - 9, x + 9, y + 1, C_TEXT);
  lcd.fillRect(x - 6, y + 1, 12, 8, C_TEXT);
}

static void menuDraw();
static void brightDraw();
static void infoDraw();
static bool eyeEnter();
static void eyeExit();
static void matrixEnter();
static void chatEnter();
static void gifEnter();
static void drawEnter();

static void gotoApp(uint8_t a) {
  swallow = tp.down;
  lastTouchMs = millis();
  app = (App)a;
  switch (a) {
    case A_MENU:   lcd.setBrightness(brightness); menuDraw(); break;
    case A_CHAT:   chatEnter(); break;
    case A_MATRIX: matrixEnter(); break;
    case A_BRIGHT: brightDraw(); break;
    case A_INFO:   infoDraw(); break;
    case A_SLEEP:  lcd.fillScreen(C_BG); lcd.setBrightness(0); break;
    case A_GIF:    gifEnter(); break;
    case A_DRAW:   drawEnter(); break;
    default: break;
  }
}

// ================================ MENU =====================================
struct Item { const char* name; App app; uint16_t top, bot; };
static const Item kItems[] = {
  { "Mat AI",    A_EYE,    RGB565( 90,180,255), RGB565( 10, 90,200) },
  { "Chat AI",   A_CHAT,   RGB565(255,160,205), RGB565(190, 40,130) },
  { "Matrix",    A_MATRIX, RGB565(120,230,120), RGB565( 10,140, 45) },
  { "Do sang",   A_BRIGHT, RGB565(255,225,100), RGB565(240,140,  0) },
  { "Thong tin", A_INFO,   RGB565(200,160,255), RGB565(100, 60,200) },
  { "Ngu",       A_SLEEP,  RGB565(140,210,235), RGB565( 40,100,170) },
  { "Anh GIF",   A_GIF,    RGB565(255,170,120), RGB565(210, 70, 20) },
  { "Ve",        A_DRAW,   RGB565(255,130,190), RGB565(200, 40,120) },
};
// mau icon cho theme Sakura (cung thu tu voi kItems)
static const uint16_t kSakura[8][2] = {
  { RGB565(255,205,180), RGB565(228, 95, 92) },
  { RGB565(255,215,195), RGB565(236,112,112) },
  { RGB565(248,195,200), RGB565(188, 88,120) },
  { RGB565(255,232,185), RGB565(245,150, 88) },
  { RGB565(250,205,210), RGB565(192,104,140) },
  { RGB565(238,205,215), RGB565(152,102,150) },
  { RGB565(255,210,178), RGB565(236,112, 80) },
  { RGB565(255,200,215), RGB565(214, 84,120) },
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
    case A_EYE: {
      // Đôi mắt Anime tròng đỏ ruby, có chuyển sắc trên-dưới, mi mắt dày và đốm sáng lấp lánh
      for (int s = -1; s <= 1; s += 2) {
        int ex = cx + s * 17;
        // 1. Lông mày mảnh phía trên
        lcd.drawLine(ex - s * 10, cy - 17, ex + s * 9, cy - 15, RGB565(45, 25, 35));
        lcd.drawLine(ex - s * 10, cy - 16, ex + s * 9, cy - 14, RGB565(45, 25, 35));

        // 2. Viền mi mắt đen (dày ở trên, vểnh đuôi mắt ra ngoài)
        lcd.fillRoundRect(ex - 13, cy - 12, 26, 23, 9, RGB565(20, 12, 22));
        lcd.fillTriangle(ex + s * 10, cy - 11, ex + s * 16, cy - 14, ex + s * 12, cy - 4, RGB565(20, 12, 22));

        // 3. Lòng trắng mắt
        lcd.fillRoundRect(ex - 11, cy - 8, 22, 18, 7, C_TEXT);

        // 4. Tròng mắt ĐỎ Anime (3 lớp: viền đỏ sẫm -> đỏ tươi -> vầng sáng hồng đỏ ở đáy)
        lcd.fillEllipse(ex, cy + 1, 8, 9, RGB565(135, 10, 25));      // Viền tròng đỏ đậm
        lcd.fillEllipse(ex, cy + 1, 7, 8, RGB565(235, 25, 45));      // Nền đỏ tươi
        lcd.fillEllipse(ex, cy - 3, 7, 4, RGB565(155, 10, 30));      // Bóng tối nửa trên tròng mắt
        lcd.fillEllipse(ex, cy + 5, 5, 3, RGB565(255, 115, 125));    // Ánh đỏ hồng rực ở đáy mắt

        // 5. Đồng tử (con ngươi) đen + tâm đỏ sẫm ở giữa kiểu anime
        lcd.fillEllipse(ex, cy + 1, 3, 4, RGB565(25, 5, 15));
        lcd.fillCircle(ex, cy + 1, 1, RGB565(190, 20, 35));

        // 6. Đốm sáng trắng (Highlight) lớn & nhỏ tạo hồn cho mắt
        lcd.fillCircle(ex - 4, cy - 3, 3, C_TEXT);
        lcd.fillCircle(ex + 4, cy + 4, 1, C_TEXT);
        lcd.drawPixel(ex + 3, cy - 4, C_TEXT);

        // 7. Má hồng 2 bên có vệt gạch chéo (blush)
        lcd.fillEllipse(ex + s * 9, cy + 14, 5, 2, RGB565(255, 145, 165));
      }
      break;
    }
    case A_CHAT: {
      // 2 Bong bóng chat xếp lớp + sao lấp lánh AI
      uint16_t sub = mix(C_TEXT, bot, 75);
      lcd.fillRoundRect(cx - 6, cy - 16, 30, 20, 7, sub);
      lcd.fillTriangle(cx + 14, cy + 4, cx + 21, cy + 4, cx + 23, cy + 10, sub);
      lcd.fillRoundRect(cx - 24, cy - 8, 34, 22, 8, C_TEXT);
      lcd.fillTriangle(cx - 16, cy + 14, cx - 8, cy + 14, cx - 19, cy + 21, C_TEXT);
      // Ngôi sao 4 cánh lấp lánh (AI Sparkle)
      int sx = cx - 7, sy = cy + 3;
      lcd.fillTriangle(sx, sy - 7, sx - 2, sy, sx + 2, sy, bot);
      lcd.fillTriangle(sx, sy + 7, sx - 2, sy, sx + 2, sy, bot);
      lcd.fillTriangle(sx - 7, sy, sx, sy - 2, sx, sy + 2, bot);
      lcd.fillTriangle(sx + 7, sy, sx, sy - 2, sx, sy + 2, bot);
      lcd.fillCircle(sx + 9, sy - 5, 1, bot);
      break;
    }
    case A_MATRIX: {
      // Mưa mã rơi có vệt sáng trắng ở đầu và đuôi xanh lá mờ dần
      lcd.fillRoundRect(cx - 24, cy - 17, 48, 34, 7, RGB565(8, 28, 14));
      lcd.drawRoundRect(cx - 24, cy - 17, 48, 34, 7, RGB565(70, 220, 100));
      static const int8_t hy[7] = { 8, -3, 11, 2, 12, -5, 6 };
      static const int8_t ln[7] = { 16, 11, 21, 14, 19, 10, 15 };
      for (int c = 0; c < 7; c++) {
        int x = cx - 18 + c * 6, y = cy + hy[c], l = ln[c];
        lcd.fillRect(x, y - l, 3, l - 6, RGB565(0, 95, 30));
        lcd.fillRect(x, y - 6, 3, 4, RGB565(40, 225, 85));
        lcd.fillRect(x, y - 2, 3, 3, C_TEXT);
      }
      break;
    }
    case A_BRIGHT: {
      // Mặt trời có 8 tia tam giác nhọn + tâm phát sáng nhiều lớp
      for (int a = 0; a < 8; a++) {
        float r = a * 0.7854f, r1 = r - 0.22f, r2 = r + 0.22f;
        lcd.fillTriangle(cx + cosf(r) * 21,  cy + sinf(r) * 21,
                         cx + cosf(r1) * 13, cy + sinf(r1) * 13,
                         cx + cosf(r2) * 13, cy + sinf(r2) * 13, C_TEXT);
      }
      lcd.fillCircle(cx, cy, 11, C_TEXT);
      lcd.fillCircle(cx, cy, 8, RGB565(255, 220, 70));
      lcd.fillCircle(cx - 3, cy - 3, 2, C_TEXT);
      break;
    }
    case A_INFO: {
      // Chip vi xử lý (CPU) có các chân kim loại xung quanh
      uint16_t pinC = RGB565(255, 230, 130);
      for (int k = -1; k <= 1; k++) {
        lcd.fillRect(cx + k * 7 - 2, cy - 19, 4, 5, pinC);
        lcd.fillRect(cx + k * 7 - 2, cy + 14, 4, 5, pinC);
        lcd.fillRect(cx - 19, cy + k * 7 - 2, 5, 4, pinC);
        lcd.fillRect(cx + 14, cy + k * 7 - 2, 5, 4, pinC);
      }
      lcd.fillRoundRect(cx - 14, cy - 14, 28, 28, 5, C_TEXT);
      lcd.fillRoundRect(cx - 10, cy - 10, 20, 20, 3, bot);
      lcd.drawRect(cx - 6, cy - 6, 12, 12, mix(C_TEXT, bot, 110));
      lcd.fillCircle(cx - 6, cy - 6, 1, C_TEXT);
      break;
    }
    case A_SLEEP: {
      // Trăng khuyết vàng dịu + sao lấp lánh
      uint16_t bg = mix(theme ? kSakura[5][0] : kItems[5].top, bot, 110);
      lcd.fillCircle(cx - 3, cy + 1, 15, RGB565(255, 244, 175));
      lcd.fillCircle(cx + 5, cy - 4, 13, bg);
      // Ngôi sao 4 cánh
      int sx = cx + 14, sy = cy - 9;
      lcd.fillTriangle(sx, sy - 5, sx - 1, sy, sx + 1, sy, C_TEXT);
      lcd.fillTriangle(sx, sy + 5, sx - 1, sy, sx + 1, sy, C_TEXT);
      lcd.fillTriangle(sx - 5, sy, sx, sy - 1, sx, sy + 1, C_TEXT);
      lcd.fillTriangle(sx + 5, sy, sx, sy - 1, sx, sy + 1, C_TEXT);
      lcd.fillCircle(cx + 18, cy + 5, 2, C_TEXT);
      lcd.fillCircle(cx - 17, cy - 11, 1, C_TEXT);
      break;
    }
    case A_DRAW: {
      // Bang mau: than kem, lo ngon cai, 4 cham mau
      uint16_t bg = mix(theme ? kSakura[7][0] : kItems[7].top, bot, 110);
      lcd.fillCircle(cx - 2, cy, 19, RGB565(255, 240, 215));
      lcd.fillCircle(cx + 9, cy + 9, 6, bg);
      lcd.fillCircle(cx - 11, cy - 5, 4, RGB565(230, 40, 40));
      lcd.fillCircle(cx - 2, cy - 12, 4, RGB565(255, 215, 0));
      lcd.fillCircle(cx + 9, cy - 7, 4, RGB565(40, 180, 60));
      lcd.fillCircle(cx - 12, cy + 6, 4, RGB565(40, 90, 230));
      break;
    }
    case A_GIF: {
      // Khung ảnh phong cảnh + nút Play nổi góc phải dưới
      lcd.fillRoundRect(cx - 22, cy - 16, 44, 32, 6, C_TEXT);
      lcd.fillRoundRect(cx - 19, cy - 13, 38, 26, 4, RGB565(135, 210, 255));
      lcd.fillCircle(cx - 10, cy - 5, 4, RGB565(255, 230, 95));
      lcd.fillTriangle(cx - 18, cy + 12, cx - 4, cy - 2, cx + 10, cy + 12, RGB565(70, 165, 120));
      lcd.fillTriangle(cx - 2, cy + 12, cx + 9, cy + 1, cx + 18, cy + 12, RGB565(45, 130, 95));
      lcd.fillCircle(cx + 12, cy + 7, 9, bot);
      lcd.drawCircle(cx + 12, cy + 7, 9, C_TEXT);
      lcd.fillTriangle(cx + 10, cy + 3, cx + 10, cy + 11, cx + 16, cy + 7, C_TEXT);
      break;
    }
  }
}

static void menuDraw() {
  skyBg();
  titlePill("MENU");
  for (int s = 0; s < PER_PAGE; s++) {
    int i = menuPage * PER_PAGE + s;
    if (i >= N_ITEMS) break;
    int x, y, w, h; cellRect(s, x, y, w, h);
    uint16_t ct = theme ? kSakura[i][0] : kItems[i].top;
    uint16_t cb = theme ? kSakura[i][1] : kItems[i].bot;
    glossyTile(x, y, w, h, 16, ct, cb);
    drawIcon(kItems[i].app, x + w / 2, y + 34, cb);
    label(kItems[i].name, x + w / 2, y + h - 14);
  }
  if (theme == 1) yaeDeco();
  for (int p = 0; p < N_PAGES; p++) {
    int dx = lcd.width() / 2 + (int)((p - (N_PAGES - 1) * 0.5f) * 16), dy = lcd.height() - 8;
    if (p == menuPage) { lcd.fillCircle(dx, dy, 5, C_TEXT); lcd.drawCircle(dx, dy, 5, C_DARK); }
    else                 lcd.fillCircle(dx, dy, 4, TH.pillBot);
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
  if (tp.sy < 34) { theme ^= 1; menuDraw(); return; }        // cham thanh MENU = doi theme
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
  lcd.startWrite();
  lcd.setClipRect(SL_X0 - 16, SL_Y - 8, SL_X1 - SL_X0 + 32, 32);
  skyBg();
  lcd.clearClipRect();
  lcd.fillRoundRect(SL_X0, SL_Y, SL_X1 - SL_X0, 16, 8, RGB565(215, 238, 252));
  lcd.drawRoundRect(SL_X0, SL_Y, SL_X1 - SL_X0, 16, 8, EDGE);
  lcd.fillRoundRect(SL_X0 + 2, SL_Y + 2, fx - SL_X0, 12, 6, mix(RGB565(255, 225, 100), RGB565(240, 140, 0), 128));
  lcd.fillCircle(fx, SL_Y + 8, 13, C_TEXT);
  lcd.drawCircle(fx, SL_Y + 8, 13, EDGE);
  lcd.fillCircle(fx - 3, SL_Y + 4, 4, RGB565(225, 245, 255));
  lcd.endWrite();
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
static void infoDraw() {
  skyBg();
  titlePill("THONG TIN");
  lcd.fillRoundRect(8, 38, 304, 152, 14, C_PANEL);
  lcd.drawRoundRect(8, 38, 304, 152, 14, EDGE);
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

static void mxCell(int c, int row, uint16_t color) {
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
  if (!tp.down) mxHold = now;
  else if (now - mxHold >= 3000) { gotoApp(A_MENU); return; }
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

static int  cfgMood  = -1;
static bool talkOn   = false;
static int  speedLvl = 2;
static const float kPeriod[5] = { 2.8f, 2.0f, 1.6f, 1.1f, 0.8f };
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

static void drawGear() {
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
    ok = eyeSetBc(gL, bc, true, BC_MIRROR_L, -1) && eyeSetBc(gR, bc, true, BC_MIRROR_R, -1);
    if (ok) {
      // Dat bc theo toa do man hinh, doi xung quanh giua man hinh, khong phu thuoc vi tri mat
      const int cxs = lcd.width() / 2;
      const int pxL = lcd.width() / 2 - EYE_W - EYE_GAP / 2;     // goc trai buffer mat trai
      const int pxR = lcd.width() / 2 + (EYE_GAP - EYE_GAP / 2); // goc trai buffer mat phai
      gL.bc.x = (int16_t)(cxs - BC_GAP / 2 - pxL - gL.bc.w + BC_SHIFT_L);     // mep phai bc trai
      gR.bc.x = (int16_t)(cxs + BC_GAP / 2 - pxR + BC_SHIFT_R);             // mep trai bc phai
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
  brain.winkHold[0] = brain.winkHold[1] = false;
  freeEye(gL); freeEye(gR); freeBufs();
}

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
  glossyTile(124, 154, 40, 40, 10, ACC_T, ACC_B);
  glossyTile(236, 154, 40, 40, 10, ACC_T, ACC_B);
  label("-", 144, 174);
  label("+", 256, 174);
  char s[16]; snprintf(s, sizeof s, "Toc do %d", speedLvl + 1);
  label(s, 200, 174);
  glossyTile(lcd.width() - 56, lcd.height() - 42, 52, 36, 10, ACC_T, ACC_B);
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
  {  // cham vao mat nao thi mat do nham (tha tay ra thi mo lai)
    const int mg = 6;
    bool onL = tp.down && tp.x >= xL - mg && tp.x < xL + EYE_W + mg && tp.y >= yEye - mg && tp.y < yEye + EYE_H + mg;
    bool onR = tp.down && tp.x >= xR - mg && tp.x < xR + EYE_W + mg && tp.y >= yEye - mg && tp.y < yEye + EYE_H + mg;
    brain.winkHold[0] = onL;
    brain.winkHold[1] = onR;
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

// ============================== APP VE ======================================
// Bang ve 320x240, bo nho dem 8 bit/pixel (~77KB, chi cap phat khi mo app).
// Chon mau / co co bam nut tron o goc trai duoi (giong cai dat mat). Mau trang = TAY.
#define DR_COLORS 32
#define DR_COLS   8
// Sap xep theo thu tu mau: hang 0 = trung tinh + nau (o dau = trang = TAY);
// hang 1..3 = do, cam, vang, xanh la, ngoc, xanh duong, tim, hong (cot) x sang / thuong / toi (hang)
static const uint16_t kDrawPal[DR_COLORS] = {
  RGB565(255,255,255), RGB565(205,205,205), RGB565(150,150,150), RGB565( 90, 90, 90),
  RGB565(  0,  0,  0), RGB565(100, 60, 30), RGB565(150, 95, 50),  RGB565(250,205,160),
  // sang
  RGB565(255,150,150), RGB565(255,200,130), RGB565(255,245,150), RGB565(160,235,150),
  RGB565(150,240,240), RGB565(150,200,255), RGB565(205,160,240), RGB565(255,190,220),
  // thuong
  RGB565(230, 30, 30), RGB565(255,140,  0), RGB565(255,220,  0), RGB565( 40,180, 60),
  RGB565(  0,200,210), RGB565( 40,100,230), RGB565(140, 60,200), RGB565(240, 70,150),
  // toi
  RGB565(150, 10, 20), RGB565(190, 90,  0), RGB565(180,150,  0), RGB565(  0,100, 50),
  RGB565(  0,120,130), RGB565( 20, 50,150), RGB565( 85, 30,130), RGB565(170, 20, 90),
};
static const int8_t kBrush[4] = { 1, 2, 4, 7 };      // ban kinh net ve (px)
static uint8_t*  drBuf = nullptr;
static uint16_t  drBand[320 * 4];
static int       drW = 320, drH = 240;
static int       drColor = 4, drSize = 1, drLx = 0, drLy = 0;
static bool      drPopup = false, drStroke = false, drConfirm = false;

static inline void drPut(int x, int y, uint8_t c) {
  drBuf[y * drW + x] = c;
}

// dau tron (day dac) vao man hinh + bo nho dem
static void drStamp(int cx, int cy, int r) {
  const uint16_t col = kDrawPal[drColor];
  for (int dy = -r; dy <= r; dy++) {
    int y = cy + dy;
    if (y < 0 || y >= drH) continue;
    int half = (int)sqrtf((float)(r * r + r - dy * dy));
    int x0 = cx - half, x1 = cx + half;
    if (x0 < 0) x0 = 0;
    if (x1 >= drW) x1 = drW - 1;
    if (x0 > x1) continue;
    lcd.drawFastHLine(x0, y, x1 - x0 + 1, col);
    for (int x = x0; x <= x1; x++) drPut(x, y, (uint8_t)drColor);
  }
}

static void drLine(int x0, int y0, int x1, int y1, int r) {
  int dx = x1 - x0, dy = y1 - y0;
  int n = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
  if (n == 0) { drStamp(x0, y0, r); return; }
  int step = r > 2 ? r / 2 : 1;
  for (int i = 0; ; i += step) {
    if (i > n) i = n;
    drStamp(x0 + dx * i / n, y0 + dy * i / n, r);
    if (i == n) break;
  }
}

// 2 nut noi tren man hinh ve: bang mau (trai duoi) + home (phai duoi)
static void drBtns() {
  drawHome();
  int x = 28, y = drH - 22;
  lcd.fillCircle(x + 2, y + 3, 18, SHADOW);
  lcd.fillCircle(x, y, 18, C_TEXT);
  lcd.fillCircle(x, y, 14, kDrawPal[drColor]);
  lcd.drawCircle(x, y, 14, RGB565(90, 90, 100));
  lcd.drawCircle(x, y, 18, RGB565(90, 90, 100));
}

static bool drTouchesBtn(int x0, int y0, int x1, int y1, int m) {
  int ax = (x0 < x1 ? x0 : x1) - m, bx = (x0 > x1 ? x0 : x1) + m;
  int by = (y0 > y1 ? y0 : y1) + m;
  if (by < drH - 44) return false;
  return ax < 56 || bx >= drW - 56;
}

// ve lai toan bo bang ve tu bo nho dem
static void drRestore() {
  lcd.startWrite();
  for (int y0 = 0; y0 < drH; y0 += 4) {
    int n = (drH - y0) < 4 ? (drH - y0) : 4;
    for (int j = 0; j < n; j++) {
      const uint8_t* src = drBuf + (y0 + j) * drW;
      uint16_t* d = drBand + j * drW;
      for (int x = 0; x < drW; x++) d[x] = kDrawPal[src[x]];
    }
    lcd.pushImage(0, y0, drW, n, (const lgfx::rgb565_t*)drBand);
  }
  lcd.endWrite();
  drBtns();
}

// ---- khung chon mau / co net (toan man hinh, giong cai dat mat) ----
static void drTile(int i, int& x, int& y) { x = 18 + (i % DR_COLS) * 36; y = 36 + (i / DR_COLS) * 36; }   // o vuong 32x32

static void drPopupDraw() {
  skyBg();
  titlePill("MAU VE");
  for (int i = 0; i < DR_COLORS; i++) {
    int x, y; drTile(i, x, y);
    uint16_t c = kDrawPal[i];
    glossyTile(x, y, 32, 32, 7, mix(c, C_TEXT, 50), c);
    if (i == 0) { lcd.setFont(&fonts::Font2); lcd.setTextDatum(lgfx::middle_center); lcd.setTextColor(C_DARK); lcd.drawString("Tay", x + 16, y + 16); }
    if (i == drColor) { lcd.drawRoundRect(x - 1, y - 1, 34, 34, 8, C_TEXT); lcd.drawRoundRect(x - 2, y - 2, 36, 36, 9, C_DARK); }
  }
  for (int i = 0; i < 4; i++) {
    int x = 8 + i * 48;
    glossyTile(x, 186, 44, 36, 10, ACC_T, ACC_B);
    lcd.fillCircle(x + 22, 204, kBrush[i], C_TEXT);
    if (i == drSize) { lcd.drawRoundRect(x - 1, 185, 46, 38, 11, C_TEXT); lcd.drawRoundRect(x - 2, 184, 48, 40, 12, C_DARK); }
  }
  glossyTile(204, 186, 56, 36, 10, RGB565(255, 150, 140), RGB565(210, 50, 50));
  label("Xoa", 232, 204);
}

// hop thoai xac nhan xoa het
static void drConfirmDraw() {
  lcd.fillRoundRect(46, 66, 232, 108, 14, SHADOW);
  lcd.fillRoundRect(42, 62, 232, 108, 14, C_PANEL);
  lcd.drawRoundRect(42, 62, 232, 108, 14, EDGE);
  lcd.setFont(&fonts::Font2); lcd.setTextDatum(lgfx::middle_center); lcd.setTextColor(C_DARK);
  lcd.drawString("Xoa het tranh?", 158, 88);
  glossyTile(58, 112, 96, 40, 10, ACC_T, ACC_B);
  label("Huy", 106, 132);
  glossyTile(162, 112, 96, 40, 10, RGB565(255, 150, 140), RGB565(210, 50, 50));
  label("Xoa", 210, 132);
}

static void drPopupLoop() {
  if (!tp.releaseEdge) return;
  if (swallow) { swallow = false; return; }
  if (drConfirm) {                                       // dang hoi xac nhan xoa
    if (hit(162, 112, 96, 40)) { memset(drBuf, 0, (size_t)drW * drH); drConfirm = false; drPopup = false; drRestore(); }
    else if (hit(58, 112, 96, 40)) { drConfirm = false; drPopupDraw(); }
    return;
  }
  // chon xong la ve lai bang ve ngay (khong can nut OK)
  for (int i = 0; i < DR_COLORS; i++) {
    int x, y; drTile(i, x, y);
    if (hit(x, y, 32, 32)) { drColor = i; drPopup = false; drRestore(); return; }
  }
  for (int i = 0; i < 4; i++) {
    if (hit(8 + i * 48, 186, 44, 36)) { drSize = i; drPopup = false; drRestore(); return; }
  }
  if (hit(204, 186, 56, 36)) { drConfirm = true; drConfirmDraw(); return; }
}

static void drawEnter() {
  drW = lcd.width(); drH = lcd.height();
  if (drW > 320 || (drW & 1)) drBuf = nullptr;
  else if (!drBuf) drBuf = (uint8_t*)malloc((size_t)drW * drH);
  if (!drBuf) {
    lcd.fillScreen(C_BG);
    lcd.setFont(&fonts::Font2); lcd.setTextColor(0xF800, C_BG); lcd.setTextDatum(lgfx::top_left);
    lcd.drawString("Het RAM!", 10, 10);
    delay(1200);
    app = A_MENU; menuDraw();
    return;
  }
  memset(drBuf, 0, (size_t)drW * drH);
  drPopup = false; drStroke = false; drConfirm = false;
  lcd.fillScreen(kDrawPal[0]);
  drBtns();
}

static void drawExit() {
  free(drBuf); drBuf = nullptr;
}

static void drawLoop() {
  if (drPopup) { drPopupLoop(); delay(5); return; }       // dang mo bang mau: khong coi la nut home
  if (tp.pressEdge && inHome(tp.sx, tp.sy)) { drawExit(); gotoApp(A_MENU); return; }
  if (swallow) { if (!tp.down) swallow = false; return; }
  if (tp.pressEdge) {
    if (tp.sx < 56 && tp.sy >= drH - 44) { drPopup = true; swallow = tp.down; drPopupDraw(); return; }   // mo bang mau
    drStroke = !(tp.sy >= drH - 44 && (tp.sx < 56 || tp.sx >= drW - 56));
    drLx = tp.sx; drLy = tp.sy;
  }
  if (tp.down && drStroke && (tp.pressEdge || tp.x != drLx || tp.y != drLy)) {
    int r = kBrush[drSize];
    lcd.startWrite();
    drLine(drLx, drLy, tp.x, tp.y, r);
    lcd.endWrite();
    bool nearBtn = drTouchesBtn(drLx, drLy, tp.x, tp.y, r + 2);
    drLx = tp.x; drLy = tp.y;
    if (nearBtn) drBtns();                 // net ve cham mep nut -> ve lai nut cho khoi bi de
  }
  if (tp.releaseEdge) drStroke = false;
  delay(2);
}

// ============================== APP CHAT AI =================================
static std::vector<Msg> msgs;
static String kbText;
static bool   kbOpen = false;
static int    chatScroll = 0;
static const int CH_T = 34, CH_B = 196, CH_PAD = 7, CH_LH = 17, CH_GAP = 6, CH_MAXW = 220;
static const int CH_MAX = 12, CH_HIST = 8, KB_MAX = 200, KB_Y = 38;
static const char* const kRows[4] = { "1234567890", "qwertyuiop", "asdfghjkl,", "zxcvbnm.?!" };

static char fold(uint32_t u) {
  static const char L1[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
  static const char VN[] = "AaAaAaAaAaAaAaAaAaAaAaAaEeEeEeEeEeEeEeEeIiIiOoOoOoOoOoOoOoOoOoOoOoOoUuUuUuUuUuUuUuYyYyYyYy";
  static const uint16_t SP[] = { 0x102, 0x103, 0x110, 0x111, 0x128, 0x129, 0x168, 0x169, 0x1A0, 0x1A1, 0x1AF, 0x1B0,
                                 0x2018, 0x2019, 0x201C, 0x201D, 0x2013, 0x2014, 0x2022 };
  static const char SPC[] = "AaDdIiUuOoUu''\"\"--*";
  static_assert(sizeof(L1) == 65 && sizeof(VN) == 91 && sizeof(SPC) == sizeof(SP) / 2 + 1, "bang ky tu sai do dai");
  if (u < 0x80) return (char)u;
  if (u >= 0xC0 && u <= 0xFF) return L1[u - 0xC0];
  if (u >= 0x1EA0 && u <= 0x1EF9) return VN[u - 0x1EA0];
  for (unsigned i = 0; i < sizeof(SP) / 2; i++) if (SP[i] == u) return SPC[i];
  return 0;
}
static String ascii(const char* s) {
  String o;
  while (*s) {
    uint32_t u = (uint8_t)*s++;
    if (u >= 0xC0) {
      int n = u >= 0xF0 ? 3 : (u >= 0xE0 ? 2 : 1);
      u &= 0x3F >> n;
      while (n-- && (*s & 0xC0) == 0x80) u = (u << 6) | (*s++ & 0x3F);
    } else if (u >= 0x80) continue;
    if (char c = fold(u)) o += c;
  }
  return o;
}

static void layout(Msg& m) {
  lcd.setFont(&fonts::Font2);
  String line;
  m.wr = ""; m.w = 0; m.lines = 0;
  auto flush = [&]() {
    if (m.lines++) m.wr += '\n';
    m.wr += line;
    int tw = lcd.textWidth(line.c_str());
    if (tw > m.w) m.w = tw;
    line = "";
  };
  for (unsigned i = 0; i < m.t.length(); i++) {
    char c = m.t[i];
    if (c == '\n') { flush(); continue; }
    line += c;
    if (lcd.textWidth(line.c_str()) <= CH_MAXW) continue;
    int sp = line.lastIndexOf(' ');
    int cut = sp > 0 ? sp : (int)line.length() - 1;
    String rest = line.substring(sp > 0 ? sp + 1 : cut);
    line.remove(cut);
    flush();
    line = rest;
  }
  if (line.length() || !m.lines) flush();
}

static int msgH(const Msg& m) { return m.lines * CH_LH + 2 * CH_PAD; }
static int chatTotal() { int s = 0; for (auto& m : msgs) s += msgH(m) + CH_GAP; return s; }

static void addMsg(bool me, const String& t, bool err = false) {
  Msg m; m.me = me; m.err = err; m.t = t;
  layout(m);
  msgs.push_back(m);
  if ((int)msgs.size() > CH_MAX) msgs.erase(msgs.begin());
}

static void chatDraw() {
  const int W = lcd.width();
  skyBg();
  titlePill("CHAT AI");
  if (msgs.empty()) label("Cham thanh ben duoi de chat voi AI", W / 2, 115);
  lcd.setClipRect(0, CH_T, W, CH_B - CH_T);
  lcd.setFont(&fonts::Font2);
  lcd.setTextDatum(lgfx::top_left);
  int y = CH_B + chatScroll;
  for (int i = (int)msgs.size() - 1; i >= 0; i--) {
    const Msg& m = msgs[i];
    int bh = msgH(m), bw = m.w + 2 * CH_PAD;
    y -= bh + CH_GAP;
    if (y >= CH_B) continue;
    if (y + bh <= CH_T) break;
    int x = m.me ? W - 8 - bw : 8;
    lcd.fillRoundRect(x, y, bw, bh, 12, m.me ? MINE : (m.err ? RGB565(255, 215, 215) : RGB565(236, 240, 245)));
    lcd.setTextColor(m.me ? C_TEXT : (m.err ? RGB565(150, 20, 20) : RGB565(20, 30, 45)));
    int ty = y + CH_PAD;
    for (int p = 0; p < (int)m.wr.length(); ) {
      int e = m.wr.indexOf('\n', p);
      if (e < 0) e = m.wr.length();
      lcd.drawString(m.wr.substring(p, e).c_str(), x + CH_PAD, ty);
      ty += CH_LH; p = e + 1;
    }
  }
  lcd.clearClipRect();
  int h = lcd.height();
  glossyTile(11, h - 39, 34, 34, 17, RGB565(255, 150, 140), RGB565(200, 40, 40));
  label("Xoa", 28, h - 22);
  lcd.fillRoundRect(58, 200, 200, 34, 17, C_TEXT);
  lcd.drawRoundRect(58, 200, 200, 34, 17, EDGE);
  lcd.setFont(&fonts::Font2);
  lcd.setTextDatum(lgfx::middle_left);
  lcd.setTextColor(RGB565(130, 150, 170));
  lcd.drawString("Nhap tin nhan...", 74, 217);
  drawHome();
}

static bool chatAsk(String& out) {
  if (WiFi.status() != WL_CONNECTED) {
    for (uint32_t t = millis(); WiFi.status() != WL_CONNECTED && millis() - t < 25000; ) delay(100);
    if (WiFi.status() != WL_CONNECTED) {
      out = "WiFi loi, ma " + String((int)WiFi.status()) + " (1=khong thay ten WiFi, 4=sai mat khau)";
      return false;
    }
  }
  JsonDocument req;
  req["model"] = API_MODEL;
  JsonArray arr = req["messages"].to<JsonArray>();
  JsonObject sys = arr.add<JsonObject>();
  sys["role"] = "system"; sys["content"] = SYS_PROMPT;
  size_t last = msgs.size() - 1;
  for (size_t i = last > CH_HIST ? last - CH_HIST : 0; i < last; i++) {
    if (msgs[i].err) continue;
    JsonObject o = arr.add<JsonObject>();
    o["role"] = msgs[i].me ? "user" : "assistant";
    o["content"] = msgs[i].t;
  }
  String body;
  serializeJson(req, body);

  static WiFiClientSecure tls;
  static WiFiClient plain;
  tls.setInsecure();
  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(20000);
  if (!http.begin(strncmp(API_URL, "https", 5) == 0 ? (WiFiClient&)tls : plain, API_URL)) { out = "Link API sai"; return false; }
  http.addHeader("Content-Type", "application/json");
  if (*API_KEY) http.addHeader("Authorization", String("Bearer ") + API_KEY);

  bool ok = false;
  int code = http.POST(body);
  if (code <= 0) out = "Loi mang: " + http.errorToString(code);
  else {
    JsonDocument filter, res;
    filter["choices"][0]["message"]["content"] = true;
    filter["error"]["message"] = true;
    if (deserializeJson(res, http.getStream(), DeserializationOption::Filter(filter))) out = "Loi doc phan hoi (" + String(code) + ")";
    else if (code != 200) out = "Loi " + String(code) + ": " + String(res["error"]["message"] | "");
    else {
      out = res["choices"][0]["message"]["content"] | "";
      ok = out.length() > 0;
      if (!ok) out = "Phan hoi rong";
    }
  }
  http.end();
  return ok;
}

static void chatSend() {
  addMsg(true, kbText);
  addMsg(false, "...");
  kbText = ""; kbOpen = false; chatScroll = 0; swallow = tp.down;
  chatDraw();
  String r;
  bool ok = chatAsk(r);
  Msg& p = msgs.back();
  p.t = ascii(r.c_str()); p.err = !ok;
  layout(p);
  chatDraw();
  lastTouchMs = millis();
}

static void kbBox() {
  lcd.fillRoundRect(6, 4, 308, 30, 10, C_TEXT);
  lcd.drawRoundRect(6, 4, 308, 30, 10, EDGE);
  String s = kbText + "_";
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(C_DARK);
  bool ov = lcd.textWidth(s.c_str()) > 292;
  lcd.setClipRect(12, 4, 296, 30);
  lcd.setTextDatum(ov ? lgfx::middle_right : lgfx::middle_left);
  lcd.drawString(s.c_str(), ov ? 308 : 12, 19);
  lcd.clearClipRect();
}
static void kbKey(int x, int y, int w, const char* s, uint16_t top, uint16_t bot) {
  glossyTile(x, y, w - 3, 30, 9, top, bot);
  label(s, x + (w - 3) / 2, y + 15);
}
static void kbDraw() {
  skyBg();
  kbBox();
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 10; c++) {
      char s[2] = { kRows[r][c], 0 };
      kbKey(c * 32, KB_Y + r * 34, 32, s, ACC_T, ACC_B);
    }
  const int y = KB_Y + 4 * 34;
  kbKey(0,   y, 64,  "Huy",  RGB565(255, 150, 140), RGB565(200, 40, 40));
  kbKey(64,  y, 64,  "Xoa",  RGB565(255, 225, 100), RGB565(240, 140, 0));
  kbKey(128, y, 128, "Cach", RGB565(235, 225, 255), TH.pillBot);
  kbKey(256, y, 64,  "Gui",  RGB565(130, 235, 140), RGB565(20, 150, 60));
}
static void kbLoop() {
  if (!tp.pressEdge || tp.sy < KB_Y) return;
  int r = (tp.sy - KB_Y) / 34;
  if (r > 4) return;
  if (r < 4) {
    if (kbText.length() < KB_MAX) kbText += kRows[r][min(tp.sx / 32, 9)];
  } else {
    int k = tp.sx / 64;
    if (k == 0) { kbOpen = false; swallow = tp.down; chatDraw(); return; }
    if (k == 4) { if (kbText.length()) chatSend(); return; }
    if (k == 1) { if (kbText.length()) kbText.remove(kbText.length() - 1); }
    else if (kbText.length() < KB_MAX) kbText += ' ';
  }
  kbBox();
}

static void chatEnter() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    delay(150);
    esp_wifi_set_mac(WIFI_IF_STA, WIFI_MAC);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
  kbOpen = false; chatScroll = 0;
  chatDraw();
}

static void chatLoop() {
  if (kbOpen) { kbLoop(); return; }
  if (tp.pressEdge && inHome(tp.sx, tp.sy)) {
    if (WIFI_OFF_ON_EXIT) { WiFi.disconnect(true); delay(150); WiFi.mode(WIFI_OFF); delay(150); }
    gotoApp(A_MENU); return;
  }
  if (!tp.releaseEdge) return;
  if (swallow) { swallow = false; return; }
  int dx = tp.x - tp.sx, dy = tp.y - tp.sy;
  if (abs(dy) > 30 && abs(dy) > abs(dx)) {
    int mx = max(0, chatTotal() - (CH_B - CH_T));
    chatScroll = constrain(chatScroll + (dy > 0 ? 90 : -90), 0, mx);
    chatDraw();
    return;
  }
  if (abs(dx) > 18 || abs(dy) > 18 || tp.sy < CH_B) return;
  if (inGear(tp.sx, tp.sy))                      { msgs.clear(); chatScroll = 0; chatDraw(); }
  else if (tp.sx >= 58 && tp.sx < 258)           { kbOpen = true; kbDraw(); }
}

// ============================== APP ANH GIF =================================
// Chap nhan GIF bat ky kich thuoc <= 320x240, tu can giua. Chinh mau o GAIN_* va SAT.
static AnimatedGIF gif;
static uint16_t gifLine[320];
static uint16_t gifPal[256];
static int  gifX = 0, gifY = 0;
static bool gifOk = false, gifOverlapHome = false;
static const uint16_t GIF_BG = 0xFFFF;             // mau nen (trang). 0x0000 = den

// 256 = giu nguyen. Tang GAIN_G / giam GAIN_B de bot tim. Tang cac gain de sang hon.
static const int GAIN_R = 282, GAIN_G = 302, GAIN_B = 270;
static const int SAT    = 300;                      // do dam mau: 256 = goc
static inline uint16_t gifColor(const uint8_t* c) {
  int r = (c[0] * GAIN_R) >> 8, g = (c[1] * GAIN_G) >> 8, b = (c[2] * GAIN_B) >> 8;
  int l = (r * 77 + g * 150 + b * 29) >> 8;
  r = l + (((r - l) * SAT) >> 8) + 4;
  g = l + (((g - l) * SAT) >> 8) + 2;
  b = l + (((b - l) * SAT) >> 8) + 4;
  r = r < 0 ? 0 : (r > 255 ? 255 : r);
  g = g < 0 ? 0 : (g > 255 ? 255 : g);
  b = b < 0 ? 0 : (b > 255 ? 255 : b);
  return lcd.color565(r, g, b);
}

static void gifDraw(GIFDRAW* d) {
  // Chi tinh bang mau 256 mau dung 1 lan o dong dau tien cua moi frame
  if (d->y == 0 && d->pPalette24) {
    const uint8_t* p = d->pPalette24;
    for (int i = 0; i < 256; i++, p += 3) gifPal[i] = gifColor(p);
  }
  int y = gifY + d->iY + d->y;
  if (y < 0 || y >= lcd.height()) return;
  const int W = lcd.width();
  uint8_t* s = d->pPixels;
  int w = d->iWidth, x0 = gifX + d->iX;
  bool tr = d->ucHasTransparency;
  bool dis = d->ucDisposalMethod == 2;

  int x = 0;
  while (x < w) {
    int sx = x0 + x;
    bool isTr = tr && s[x] == d->ucTransparent;
    if (sx < 0 || sx >= W || (isTr && !dis)) { x++; continue; }
    int st = x, n = 0;
    while (x < w && x0 + x < W) {
      bool t = tr && s[x] == d->ucTransparent;
      if (t && !dis) break;
      gifLine[n++] = t ? GIF_BG : gifPal[s[x]];
      x++;
    }
    lcd.pushImage(x0 + st, y, n, 1, (const lgfx::rgb565_t*)gifLine);
  }
}

static void gifEnter() {
  lcd.fillScreen(GIF_BG);
  gif.begin(GIF_PALETTE_RGB888);
  gifOk = gif.open((uint8_t*)my_gif, sizeof(my_gif), gifDraw);
  if (gifOk) {
    int cw = gif.getCanvasWidth(), ch = gif.getCanvasHeight();
    gifX = (lcd.width()  - cw) / 2;
    gifY = (lcd.height() - ch) / 2;
    gifOverlapHome = (gifX + cw > lcd.width() - 48) && (gifY + ch > lcd.height() - 42);
  } else {
    lcd.setFont(&fonts::Font2); lcd.setTextColor(0xF800); lcd.setTextDatum(lgfx::middle_center);
    lcd.drawString("Khong mo duoc GIF", lcd.width() / 2, lcd.height() / 2);
  }
  drawHome();
}

static void gifLoop() {
  if (tp.pressEdge && inHome(tp.sx, tp.sy)) { if (gifOk) gif.close(); gotoApp(A_MENU); return; }
  if (!gifOk) return;
  lcd.startWrite();
  bool more = gif.playFrame(true, NULL);
  lcd.endWrite();
  if (!more) gif.reset();
  if (gifOverlapHome) drawHome();
}

// ========================= MAN HINH CHO: NYAN CAT ===========================
#define IDLE_MS  120000UL
#define NY_BAND  8
static bool     nyanOn = false;
static uint32_t nyanLast = 0;
static uint8_t  nyanF = 0;
static int      nySrcY0 = 0, nySrcH = NYAN_H;
static uint8_t  nyXmap[320];
static uint16_t nyBand[320 * NY_BAND];

static void nyanEnter() {
  const int W = min((int)lcd.width(), 320), H = lcd.height();
  nySrcH  = NYAN_W * H / W;
  if (nySrcH > NYAN_H) nySrcH = NYAN_H;
  nySrcY0 = (NYAN_H - nySrcH) / 2;
  for (int x = 0; x < W; x++) nyXmap[x] = x * NYAN_W / W;
  nyanF = 0; nyanLast = 0;
  lastTouchMs = millis();
  nyanOn = true;
}

static void nyanDrawFrame(int f) {
  const int W = min((int)lcd.width(), 320), H = lcd.height();
  const uint8_t* fr = nyan_px[f];
  lcd.startWrite();
  for (int y0 = 0; y0 < H; y0 += NY_BAND) {
    int bh = min(NY_BAND, H - y0);
    for (int r = 0; r < bh; r++) {
      const uint8_t* row = fr + (nySrcY0 + (y0 + r) * nySrcH / H) * NYAN_W;
      uint16_t* o = nyBand + r * W;
      for (int x = 0; x < W; x++) o[x] = nyan_pal[row[nyXmap[x]]];
    }
    lcd.pushImage(0, y0, W, bh, (const lgfx::rgb565_t*)nyBand);
  }
  lcd.endWrite();
}

static void nyanExit() {
  nyanOn = false;
  ignoreTouch = true;
  lastTouchMs = millis();
  switch (app) {
    case A_MENU:   menuDraw();   break;
    case A_BRIGHT: brightDraw(); break;
    case A_INFO:   infoDraw();   break;
    case A_CHAT:   chatDraw();   break;
    default: break;
  }
}

static void nyanStep() {
  uint32_t now = millis();
  if (now - nyanLast < NYAN_FRAME_MS) { delay(5); return; }
  nyanLast = now;
  nyanDrawFrame(nyanF);
  if (++nyanF >= NYAN_N) nyanF = 0;
}

static bool nyanAllowed() {
  return app == A_MENU || app == A_BRIGHT || app == A_INFO || (app == A_CHAT && !kbOpen);
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
  if (nyanOn) {
    if (tp.pressEdge) nyanExit(); else nyanStep();
    return;
  }
  if (nyanAllowed() && !tp.down && millis() - lastTouchMs >= IDLE_MS) {
    nyanEnter();
    if (nyanOn) return;
  }
  switch (app) {
    case A_MENU:   menuLoop();   break;
    case A_EYE:    eyeLoop();    return;
    case A_CHAT:   chatLoop();   break;
    case A_MATRIX: matrixLoop(); break;
    case A_BRIGHT: brightLoop(); break;
    case A_INFO:   infoLoop();   break;
    case A_SLEEP:  if (tp.pressEdge) { gotoApp(A_MENU); } break;
    case A_GIF:    gifLoop();    return;
    case A_DRAW:   drawLoop();   return;
  }
  delay(10);
}