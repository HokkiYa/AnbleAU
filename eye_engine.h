// ============================================================================
//  eye_engine.h  -  Engine ve mat + mieng + animation "khuon mat AI"
//  Khong phu thuoc Arduino/man hinh -> co the test tren may tinh.
//
//  Cac lop cua MOI mat (duoi -> tren):
//     long trang SCLERA (base) < mong mat do (iris) < con nguoi den (pupil)
//     < diem sang (glint) < LMAY (la mi mat) < BC (long may bieu cam)
//
//  * SCLERA: long trang, dung yen, chi lam "khung" cho mat. Iris/pupil/glint
//    duoc cat (clip) theo vung sclera + iris luc nghi nen khong tran ra ngoai.
//  * LMAY la mi mat: gap xuong de nham mat, nang len de mo mat.
//  * BC chi dung de bieu cam (nhiu may / nang may / nghieng may).
//  * Mieng: 1 khung 120x90, doi khung theo thoi gian.
// ============================================================================
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef EYE_W
#define EYE_W 120
#endif
#ifndef EYE_H
#define EYE_H 90
#endif

// 1 = luu pixel dang big-endian (dung cho DMA gui thang len ST7789)
#ifndef EYE_SWAP_BYTES
#define EYE_SWAP_BYTES 1
#endif

#ifndef EYE_BG
#define EYE_BG  0xFFFF
#endif
#ifndef EYE_KEY
#define EYE_KEY 0xFFFF
#endif

#define EYE_OPT __attribute__((optimize("O3")))

// ----------------------------------------------------------------------------
//  Phan 1: du lieu + ve
// ----------------------------------------------------------------------------
struct EyeLayer {
  uint16_t* px;       // pixel da cat gon (EYE_KEYI = trong suot)
  int16_t w, h;       // kich thuoc sau khi cat
  int16_t x, y;       // vi tri nghi (rest) trong khung 120x90
};

struct EyeSet {
  uint16_t* base;                         // long trang (sclera) nguyen khung 120x90
  bool      hasBase;
  EyeLayer  iris, pupil, glint, lid, bc;  // lid = lmay (mi mat), bc = long may bieu cam
  int16_t   mx0[EYE_H], mx1[EYE_H];       // vung mat moi hang (sclera + iris luc nghi), lo>hi = hang rong
  int16_t   cx;                           // tam ngang cua mat
  int16_t*  lidLow;                       // moi cot cua lid: hang thap nhat co pixel, -1 = khong co
  int16_t   lidMax;                       // max(lidLow)
  int16_t   closeDY;                      // lid phai gap xuong bao nhieu px de nham han mat
  int16_t   irisTop, irisBot, irisCx;     // vung mong mat luc nghi
};

struct EyeRender {                // thong so 1 frame cua 1 mat (toan so nguyen)
  int16_t idx, idy;               // dich cua mong mat (iris)
  int16_t pdx, pdy;               // dich cua con nguoi (pupil)
  int16_t gdx, gdy;               // dich cua diem sang (glint)
  int16_t ldx, ldy;               // dich cua lmay = mi mat (ldy > 0: gap xuong)
  int16_t bdx, bdy;               // dich cua long may bieu cam (bc)
  int16_t bk;                     // do nghieng bc (1/64), da nhan huong
};

static inline uint16_t eyeSwap16(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }
static inline uint16_t eyeToNative(uint16_t v, bool be) { return be ? eyeSwap16(v) : v; }
static inline uint16_t eyeConv(uint16_t v) {
#if EYE_SWAP_BYTES
  return eyeSwap16(v);
#else
  return v;
#endif
}
#define EYE_KEYI (eyeConv(EYE_KEY))
#define EYE_BGI  (eyeConv(EYE_BG))

// Cat gon 1 layer 120x90 (bo phan trong suot). mirror = lat ngang truoc khi cat.
static bool eyeCrop(EyeLayer& L, const uint16_t* src, bool be, bool mirror = false) {
  int x0 = EYE_W, y0 = EYE_H, x1 = -1, y1 = -1;
  for (int y = 0; y < EYE_H; y++)
    for (int x = 0; x < EYE_W; x++) {
      int sx = mirror ? (EYE_W - 1 - x) : x;
      if (eyeToNative(src[y * EYE_W + sx], be) != EYE_KEY) {
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
      }
    }
  if (x1 < 0) { L.px = nullptr; L.w = L.h = L.x = L.y = 0; return true; }   // layer rong
  L.w = x1 - x0 + 1; L.h = y1 - y0 + 1; L.x = x0; L.y = y0;
  L.px = (uint16_t*)malloc((size_t)L.w * L.h * 2);
  if (!L.px) return false;
  for (int y = 0; y < L.h; y++)
    for (int x = 0; x < L.w; x++) {
      int sx = mirror ? (EYE_W - 1 - (x0 + x)) : (x0 + x);
      L.px[y * L.w + x] = eyeConv(eyeToNative(src[(y0 + y) * EYE_W + sx], be));
    }
  return true;
}

// Dung du lieu 1 mat tu 5 layer: TRANG (sclera), mong mat, con nguoi, diem sang, lmay
//   be = true neu du lieu doc tu file .bin big-endian (eye.bin)
static bool eyeBuild(EyeSet& e, const uint16_t* sclera, const uint16_t* iris,
                     const uint16_t* pupil, const uint16_t* glint, const uint16_t* lmay,
                     bool be = true) {
  memset(&e, 0, sizeof(e));

  // --- lop trang: giu nguyen ca khung 120x90 ---
  e.base = (uint16_t*)malloc((size_t)EYE_W * EYE_H * 2);
  if (!e.base) return false;
  e.hasBase = false;
  for (int i = 0; i < EYE_W * EYE_H; i++) {
    uint16_t v = eyeToNative(sclera[i], be);
    if (v != EYE_KEY) { e.hasBase = true; e.base[i] = eyeConv(v); }
    else              e.base[i] = EYE_BGI;
  }
  if (!e.hasBase) { free(e.base); e.base = nullptr; }

  if (!eyeCrop(e.iris, iris, be) || !eyeCrop(e.pupil, pupil, be) ||
      !eyeCrop(e.glint, glint, be) || !eyeCrop(e.lid, lmay, be)) return false;

  // --- vung mat moi hang = sclera + iris luc nghi ---
  int xmin = EYE_W, xmax = -1, top = EYE_H, bot = -1;
  if (e.hasBase) {
    for (int y = 0; y < EYE_H; y++) {
      int lo = EYE_W, hi = -1;
      for (int x = 0; x < EYE_W; x++) {
        bool on = eyeToNative(sclera[y * EYE_W + x], be) != EYE_KEY;
        if (!on && e.iris.px) {
          int ix = x - e.iris.x, iy = y - e.iris.y;
          if (ix >= 0 && ix < e.iris.w && iy >= 0 && iy < e.iris.h)
            on = e.iris.px[iy * e.iris.w + ix] != EYE_KEYI;
        }
        if (on) { if (x < lo) lo = x; if (x > hi) hi = x; }
      }
      e.mx0[y] = (int16_t)lo; e.mx1[y] = (int16_t)hi;
      if (hi >= 0) {
        if (y < top) top = y;
        bot = y;
        if (lo < xmin) xmin = lo;
        if (hi > xmax) xmax = hi;
      }
    }
    e.cx = (int16_t)((xmin + xmax) / 2);
  } else {
    for (int y = 0; y < EYE_H; y++) { e.mx0[y] = 0; e.mx1[y] = EYE_W - 1; }
    e.cx = EYE_W / 2;
  }

  if (e.iris.px) {
    e.irisTop = e.iris.y; e.irisBot = (int16_t)(e.iris.y + e.iris.h - 1);
    e.irisCx = (int16_t)(e.iris.x + e.iris.w / 2);
  } else { e.irisTop = 10; e.irisBot = EYE_H - 10; e.irisCx = EYE_W / 2; }

  // --- mep duoi cua lmay theo tung cot ---
  if (e.lid.px) {
    e.lidLow = (int16_t*)malloc((size_t)e.lid.w * sizeof(int16_t));
    if (!e.lidLow) return false;
    e.lidMax = -1;
    for (int c = 0; c < e.lid.w; c++) {
      int low = -1;
      for (int r = e.lid.h - 1; r >= 0; r--)
        if (e.lid.px[r * e.lid.w + c] != EYE_KEYI) { low = e.lid.y + r; break; }
      e.lidLow[c] = (int16_t)low;
      if (low > e.lidMax) e.lidMax = (int16_t)low;
    }
    // Do gap xuong de che het mat (sclera neu co, khong thi mong mat) ke ca luc dang liec xuong
    int colA, colB, botRow;
    if (e.hasBase && bot >= 0) { colA = xmin;                     colB = xmax;                     botRow = bot; }
    else                       { colA = e.irisCx - e.iris.w / 2 - 16; colB = e.irisCx + e.iris.w / 2 + 16; botRow = e.irisBot + 8; }
    int minLow = EYE_H;
    for (int x = colA; x <= colB; x++) {
      int c = x - e.lid.x;
      if (c >= 0 && c < e.lid.w && e.lidLow[c] >= 0 && e.lidLow[c] < minLow) minLow = e.lidLow[c];
    }
    if (minLow == EYE_H) minLow = e.lid.y + e.lid.h - 1;
    int need = (botRow + 1) - minLow;
    e.closeDY = (int16_t)(need > 0 ? need : 0);
  }
  return true;
}

// Gan long may bieu cam (bc). mirror = lat ngang (cho mat ben kia).
//   centerX >= 0: dat tam ngang cua bc tai day; < 0: giu vi tri goc.
static bool eyeSetBc(EyeSet& e, const uint16_t* bc, bool be, bool mirror, int centerX) {
  if (!eyeCrop(e.bc, bc, be, mirror)) return false;
  if (e.bc.px && centerX >= 0) e.bc.x = (int16_t)(centerX - e.bc.w / 2);
  return true;
}
static inline int eyeLayerCenterX(const EyeLayer& L) { return L.x + L.w / 2; }

// Ve 1 hang cua layer, chi trong khoang cot [lo, hi] cua khung
static inline void eyeOverlayRow(uint16_t* row, const EyeLayer& L, int dx, int dy, int y,
                                 int lo = 0, int hi = EYE_W - 1) {
  if (!L.px) return;
  int ly = y - (L.y + dy);
  if (ly < 0 || ly >= L.h) return;
  int ox = L.x + dx;
  int c0 = lo - ox; if (c0 < 0) c0 = 0;
  int c1 = hi - ox; if (c1 > L.w - 1) c1 = L.w - 1;
  const uint16_t* s = L.px + ly * L.w;
  for (int c = c0; c <= c1; c++) {
    uint16_t v = s[c];
    if (v != EYE_KEYI) row[ox + c] = v;
  }
}

// Long may bieu cam: giong overlay nhung co nghieng
static inline void eyeBrowRow(uint16_t* row, const EyeLayer& L, int dx, int dy, int k, int y) {
  if (!L.px) return;
  if (k == 0) { eyeOverlayRow(row, L, dx, dy, y); return; }
  const int ox = L.x + dx, bcx = ox + L.w / 2;
  int c0 = -ox; if (c0 < 0) c0 = 0;
  int c1 = EYE_W - 1 - ox; if (c1 > L.w - 1) c1 = L.w - 1;
  for (int c = c0; c <= c1; c++) {
    int ly = y - (L.y + dy) - ((k * (ox + c - bcx)) >> 6);
    if ((unsigned)ly >= (unsigned)L.h) continue;
    uint16_t v = L.px[ly * L.w + c];
    if (v != EYE_KEYI) row[ox + c] = v;
  }
}

// Ve 1 mat vao out[EYE_W*EYE_H]


#ifndef EYE_PAD_TOP
#define EYE_PAD_TOP 24      // số hàng đệm phía trên cho bc (>= BC_LIFT + vài px)
#endif

static EYE_OPT void eyeRender(const EyeSet& e, const EyeRender& p, uint16_t* out) {
  const int lidOx = e.lid.x + p.ldx;
  for (int y = -EYE_PAD_TOP; y < EYE_H; y++) {
    uint16_t* row = out + (y + EYE_PAD_TOP) * EYE_W;
    for (int i = 0; i < EYE_W; i++) row[i] = EYE_BGI;

    if (y >= 0) {
      const int lo = e.mx0[y], hi = e.mx1[y];
      if (lo <= hi) {
        if (e.hasBase) memcpy(row + lo, e.base + y * EYE_W + lo, (size_t)(hi - lo + 1) * 2);
        eyeOverlayRow(row, e.iris,  p.idx, p.idy, y, lo, hi);
        eyeOverlayRow(row, e.pupil, p.pdx, p.pdy, y, lo, hi);
        eyeOverlayRow(row, e.glint, p.gdx, p.gdy, y, lo, hi);
      }
      if (e.lid.px) {
        if (y <= e.lidMax + p.ldy) {
          for (int c = 0; c < e.lid.w; c++) {
            int x = lidOx + c;
            if ((unsigned)x < (unsigned)EYE_W && e.lidLow[c] >= 0 && e.lidLow[c] + p.ldy >= y)
              row[x] = EYE_BGI;
          }
        }
        eyeOverlayRow(row, e.lid, p.ldx, p.ldy, y);
      }
    }

    eyeBrowRow(row, e.bc, p.bdx, p.bdy, p.bk, y);   // bc vẽ cả ở hàng âm
  }
}

// Ve 1 khung mieng (120x90, du lieu big-endian tu mieng.bin) vao out
static EYE_OPT void mouthRender(const uint16_t* frame, bool be, uint16_t* out) {
  const uint16_t bg = EYE_BGI;
  for (int i = 0; i < EYE_W * EYE_H; i++) {
    uint16_t v = eyeToNative(frame[i], be);
    out[i] = (v != EYE_KEY) ? eyeConv(v) : bg;
  }
}

// ----------------------------------------------------------------------------
//  Phan 2: "bo nao" - dieu khien anh mat, chop mat, bieu cam, mieng
// ----------------------------------------------------------------------------
// Khung mieng (trung voi mieng_bin.h; eye_face.ino co the #define de ghi de)
#ifndef MOUTH_F_DEFAULT
#define MOUTH_F_DEFAULT 0
#endif
#ifndef MOUTH_F_SAD
#define MOUTH_F_SAD     1
#endif
#ifndef MOUTH_F_HAPPY
#define MOUTH_F_HAPPY   2
#endif
#ifndef MOUTH_F_BC
#define MOUTH_F_BC      8
#endif

// Thu tu khung khi noi (chay het mang roi lap lai). Sua so o day de doi nhip mieng.
//   noi luc buon ngu : 3-4-5-6 (di len roi quay xuong)
//   noi luc khac     : 3-4-5-6-7 (di len roi quay xuong)
static const uint8_t kTalkSleepy[] = { 3, 4, 5, 6, 5, 4 };
static const uint8_t kTalkNormal[] = { 3, 4, 5, 6, 7, 6, 5, 4 };

enum EyeMood : uint8_t { MOOD_NORMAL, MOOD_ANGRY, MOOD_SAD, MOOD_HAPPY, MOOD_SLEEPY, MOOD_COUNT };

// lid   : do mo cua mi mat (1 = mo het, 0 = nham)
// browDY: bc nang (-) / ha (+) ;  browTilt: bc nghieng (+ = dau trong ha thap)
struct MoodShape { float lid, gazeY, browDY, browTilt; };
static const MoodShape kMoods[MOOD_COUNT] = {
  //  lid    gazeY  browDY browTilt
  { 1.00f,  0.00f,  0.f,   0.00f },   // NORMAL
  { 0.80f,  0.00f,  4.f,   0.26f },   // ANGRY
  { 0.88f,  0.20f, -2.f,  -0.24f },   // SAD
  { 0.72f,  0.00f, -5.f,   0.00f },   // HAPPY
  { 0.40f,  0.45f,  2.f,   0.00f },   // SLEEPY
};

struct EyeBrain {
  // --- cau hinh ---
  float rangeX = 17.f, rangeY = 7.f, rangeYUp = 3.f;
  float gainIris = 1.00f, gainPupil = 1.12f, gainGlint = 0.70f;
  float saccadeTau = 0.045f;
  float mouthPeriod = 1.6f;
  float blinkClose = 0.11f, blinkHold = 0.05f, blinkOpen = 0.20f;
  bool  autoSpeak = true;
  bool  winkEnabled = true;                // bat/tat nhay mat 1 ben
  bool  winkLeftEye = true;                // true: nhay mat TRAI (gL), false: mat phai

  // --- trang thai ---
  float gx = 0, gy = 0, tx = 0, ty = 0;
  float openAmt = 1.f;
  float lidMood = 1.f;
  float cBrowDY = 0.f, cBrowTilt = 0.f, cGazeY = 0.f;
  EyeMood mood = MOOD_NORMAL;
  bool manualLook = false;
  uint32_t rng = 2463534242u;
  float saccadeT = 1.0f, microT = 0.3f, blinkT = 2.5f, blinkClock = 0.f, moodT = 12.f;
  bool blinking = false, doubleBlink = false;
  float moodHold = 0.f;
  // nhay mat 1 ben (chi luc NORMAL / HAPPY)
  // chi so 0 = mat TRAI (gL), 1 = mat PHAI (gR)
  float winkT = 7.f, winkClock[2] = {0.f, 0.f}, winkDur[2] = {0.f, 0.f}, winkAmt[2] = {1.f, 1.f};
  bool  winking[2] = {false, false};       // dang nhay (tu dong / winkNow)
  bool  winkHold[2] = {false, false};      // dang bi giu nham (cham vao mat)
  // mieng
  float speakFor = 0.f, speakClock = 0.f, speakEnd = 0.f, speakIdle = 5.f;
  bool  speaking = false, speakForever = false;

  float rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (rng & 0xFFFFFF) / 16777216.0f; }
  void seed(uint32_t s) { rng = s ? s : 2463534242u; }
  void setMood(EyeMood m) { mood = m; }
  void lookAt(float nx, float ny) { manualLook = true; tx = clampf(nx); ty = clampf(ny); }
  void releaseLook() { manualLook = false; saccadeT = 0.4f; }
  void winkNow(int eye, float sec = 0.8f) { if (!winking[eye]) { winking[eye] = true; winkClock[eye] = 0.f; winkDur[eye] = sec; } }
  static bool winkAllowed(EyeMood m) { return m == MOOD_NORMAL || m == MOOD_HAPPY; }
  void blinkNow() { if (!blinking) { blinking = true; blinkClock = 0.f; } }
  void speak(float seconds) { if (!speaking) { speaking = true; speakClock = 0.f; } speakFor = seconds; speakEnd = 0.f; }
  static float clampf(float v) { return v < -1.f ? -1.f : (v > 1.f ? 1.f : v); }
  static float smooth(float t) { t = t < 0 ? 0 : (t > 1 ? 1 : t); return t * t * (3.f - 2.f * t); }

  int mouthFrame() const {
    if (speaking) {                       // dang noi: uu tien khung noi, tru thoi theo mood
      float per = mouthPeriod < 0.2f ? 0.2f : mouthPeriod;
      const uint8_t* seq = (mood == MOOD_SLEEPY) ? kTalkSleepy : kTalkNormal;
      int n = (mood == MOOD_SLEEPY) ? (int)sizeof(kTalkSleepy) : (int)sizeof(kTalkNormal);
      int step = (int)(fmodf(speakClock, per) / per * n) % n;
      return seq[step];
    }
    if (mood == MOOD_SAD)   return MOUTH_F_SAD;
    if (mood == MOOD_HAPPY) return MOUTH_F_HAPPY;
    return MOUTH_F_DEFAULT;               // normal / angry / sleepy luc im lang
  }

  void update(float dt) {
    // --- cu liec mat ngau nhien ---
    if (!manualLook) {
      saccadeT -= dt;
      if (saccadeT <= 0.f) {
        if (rnd() < 0.35f) { tx = (rnd() - 0.5f) * 0.3f; ty = (rnd() - 0.5f) * 0.3f; }
        else               { tx = (rnd() * 2.f - 1.f);   ty = (rnd() * 2.f - 1.f) * 0.7f; }
        saccadeT = 0.7f + rnd() * 2.6f;
        if (rnd() < 0.12f) blinkNow();
      }
      microT -= dt;
      if (microT <= 0.f) {
        tx = clampf(tx + (rnd() - 0.5f) * 0.08f);
        ty = clampf(ty + (rnd() - 0.5f) * 0.08f);
        microT = 0.15f + rnd() * 0.35f;
      }
    }
    float k = 1.f - expf(-dt / saccadeTau);
    gx += (tx - gx) * k;
    gy += (ty - gy) * k;

    // --- chop mat ---
    blinkT -= dt;
    if (!blinking && blinkT <= 0.f) {
      blinkNow();
      blinkT = 3.0f + rnd() * 4.5f;
      if (rnd() < 0.15f) doubleBlink = true;
    }
    if (blinking) {
      blinkClock += dt;
      const float closeD = blinkClose, holdD = blinkHold, openD = blinkOpen;
      if (blinkClock < closeD)                openAmt = 1.f - smooth(blinkClock / closeD);
      else if (blinkClock < closeD + holdD)   openAmt = 0.f;
      else if (blinkClock < closeD + holdD + openD) openAmt = smooth((blinkClock - closeD - holdD) / openD);
      else {
        blinking = false; openAmt = 1.f;
        if (doubleBlink) { doubleBlink = false; blinkT = 0.06f; }
      }
    }

    // --- doi bieu cam ---
    if (moodHold > 0.f) moodHold -= dt;
    else {
      moodT -= dt;
      if (moodT <= 0.f) {
        float r = rnd();
        mood = r < 0.62f ? MOOD_NORMAL : (r < 0.74f ? MOOD_HAPPY : (r < 0.86f ? MOOD_SLEEPY : (r < 0.93f ? MOOD_SAD : MOOD_ANGRY)));
        moodT = (mood == MOOD_NORMAL) ? 8.f + rnd() * 14.f : 3.f + rnd() * 4.f;
      }
    }
    const MoodShape& m = kMoods[mood];
    float km = 1.f - expf(-dt * 9.f);
    lidMood   += (m.lid      - lidMood)   * km;
    cGazeY    += (m.gazeY    - cGazeY)    * km;
    cBrowDY   += (m.browDY   - cBrowDY)   * km;
    cBrowTilt += (m.browTilt - cBrowTilt) * km;

    // --- nhay mat 1 ben (khong nhay luc buon / gian / buon ngu) ---
    const int autoEye = winkLeftEye ? 0 : 1;
    if (winkEnabled && winkAllowed(mood)) {
      if (!winking[autoEye]) {
        winkT -= dt;
        if (winkT <= 0.f) { winkNow(autoEye, 0.5f + rnd() * 0.7f); winkT = 8.f + rnd() * 12.f; }
      }
    } else winking[autoEye] = false;         // doi sang mood khong cho nhay -> mo mat lai
    for (int i = 0; i < 2; i++) {
      if (winking[i]) { winkClock[i] += dt; if (winkClock[i] >= winkDur[i]) winking[i] = false; }
      bool closed = winking[i] || winkHold[i];
      winkAmt[i] += ((closed ? 0.f : 1.f) - winkAmt[i]) * (1.f - expf(-dt * 22.f));
      if (winkAmt[i] > 0.995f) winkAmt[i] = 1.f;
    }

    // --- mieng ---
    if (autoSpeak && !speaking && !speakForever && mood != MOOD_SAD) {
      speakIdle -= dt;
      if (speakIdle <= 0.f) { speak(2.0f + rnd() * 2.5f); speakIdle = 6.f + rnd() * 7.f; }
    }
    if (speaking) {
      speakClock += dt;
      if (!speakForever) {
        if (speakFor > 0.f) {
          speakFor -= dt;
          if (speakFor <= 0.f) {
            float per = mouthPeriod < 0.2f ? 0.2f : mouthPeriod;
            speakEnd = ceilf(speakClock / per) * per;
          }
        } else if (speakClock >= speakEnd) speaking = false;
      }
    }
  }

  // dir=+1: mat trai (goc trong o phia +x), dir=-1: mat phai
  void compose(const EyeSet& e, int dir, EyeRender& r) const {
    float vy = gy + cGazeY; if (vy > 1.f) vy = 1.f;
    float ox = gx * rangeX, oy = vy * (vy < 0.f ? rangeYUp : rangeY);
    r.idx = (int16_t)lroundf(ox * gainIris);  r.idy = (int16_t)lroundf(oy * gainIris);
    r.pdx = (int16_t)lroundf(ox * gainPupil); r.pdy = (int16_t)lroundf(oy * gainPupil);
    r.gdx = (int16_t)lroundf(ox * gainGlint); r.gdy = (int16_t)lroundf(oy * gainGlint);

    float myOpen = openAmt * winkAmt[dir > 0 ? 0 : 1];
    float open = lidMood * myOpen;
    r.ldx = (int16_t)lroundf(ox * 0.20f);
    r.ldy = (int16_t)lroundf((1.f - open) * e.closeDY + vy * 2.f);
    r.bdx = (int16_t)lroundf(ox * 0.15f);
    r.bdy = (int16_t)lroundf(cBrowDY - BC_LIFT + (1.f - myOpen) * 1.5f);
    //r.bdy = (int16_t)lroundf(cBrowDY + (1.f - openAmt) * 1.5f);
    r.bk  = (int16_t)lroundf(cBrowTilt * dir * 64.f);
  }
};
