#include "common.h"
#include "tftUtil.h"
#include "memo.h"
#include "net.h"
#include "settings.h"
#include "pomodoro.h"
#include "font/memo_font_16.h"

// 番茄钟：专注/短休/长休三阶段循环
// 交互：单击 开始暂停 / 双击 重置当前阶段 / 长按 回日历
// 阶段结束不等待按键，显示"完成"约2秒后自动进入下一阶段

enum PomoPhase { POMO_FOCUS, POMO_SHORT, POMO_LONG };

// 布局常量(横屏320x240)
#define DIGIT_SCALE  6                        // 8x16 ASCII 放大6倍
#define DIGIT_CELL   (8 * DIGIT_SCALE)        // 单字符宽48
#define DIGIT_TOP    50                       // 大数字顶部y
#define DIGIT_BOX_Y  46                       // 局部刷新区顶部y
#define DIGIT_BOX_H  104
#define BAR_Y        156                      // 进度条顶部y
#define BAR_X        20
#define BAR_W        280
#define BAR_H        12
#define FINISH_MS    2000                     // "完成"停留时长

static PomoPhase phase = POMO_FOCUS;
static PomoPhase pendingPhase = POMO_SHORT;
static bool running = false;                  // 是否正在倒计时
static bool finishing = false;                // 是否正在显示"完成"
static uint32_t remainMs = 0;                 // 当前阶段剩余毫秒
static uint32_t totalMs = 0;                  // 当前阶段总毫秒
static uint32_t lastMs = 0;                   // 上次计时时刻
static uint32_t finishUntil = 0;              // "完成"结束时刻
static int focusDone = 0;                     // 已完成的专注个数(累计)
static int lastShownSec = -1;                 // 上次绘制的秒数(避免重复刷新)
static uint32_t lastBarMs = 0;

static int cycleCount(){
  int cyc = cfgPomoCycle();
  return cyc < 1 ? 1 : cyc;
}
static int phaseMinutes(PomoPhase p){
  int m = (p == POMO_FOCUS) ? cfgPomoFocus() : (p == POMO_SHORT ? cfgPomoShort() : cfgPomoLong());
  return m < 1 ? 1 : m;
}
// 当前阶段配色：专注红/暂停灰/短休绿/长休青/完成橙
static uint16_t phaseColor(){
  if(finishing) return orangeColor;
  if(!running) return subColor;
  if(phase == POMO_FOCUS) return redColor;
  if(phase == POMO_SHORT) return greenColor;
  return cyanColor;
}
// 顶栏阶段名
static const char* phaseName(){
  if(!running) return "已暂停";
  if(phase == POMO_FOCUS) return "专注中";
  if(phase == POMO_SHORT) return "短休息";
  return "长休息";
}
// 第几个专注：专注中显示当前序号，休息中显示刚完成的序号
static int focusIndex(){
  int cyc = cycleCount();
  int n = focusDone % cyc;
  if(phase == POMO_FOCUS) return n + 1;
  return n == 0 ? cyc : n;
}

// 8x16 ASCII 放大绘制
static void drawBigChar(TFT_eSprite &s, int x, int y, char c, int sc, uint16_t color){
  if(c < 0x20 || c > 0x7E) return;
  for(int row = 0; row < 16; row++){
    unsigned char bits = pgm_read_byte(&memoFontAscii[c - 0x20][row]);
    for(int col = 0; col < 8; col++){
      if(bits & (0x80 >> col)) s.fillRect(x + col * sc, y + row * sc, sc, sc, color);
    }
  }
}

// 大数字 MM:SS
static void drawTimeText(TFT_eSprite &s, int y){
  uint32_t sec = (remainMs + 999) / 1000;
  char buf[10];
  snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)(sec / 60), (unsigned)(sec % 60));
  int n = strlen(buf);
  int x = (320 - n * DIGIT_CELL) / 2;
  for(int i = 0; i < n; i++) drawBigChar(s, x + i * DIGIT_CELL, y, buf[i], DIGIT_SCALE, phaseColor());
}

// 进度条 + 已过/总分钟(局部坐标 yBar 为条顶)
static void drawProgress(TFT_eSprite &s, int yBar){
  s.fillRoundRect(BAR_X, yBar, BAR_W, BAR_H, BAR_H / 2, dividerColor);
  uint32_t total = totalMs ? totalMs : 1;
  uint32_t done = totalMs > remainMs ? totalMs - remainMs : 0;
  int fw = (int)((uint64_t)done * BAR_W / total);
  if(fw > 0){
    if(fw < BAR_H) fw = BAR_H;               // 至少画满圆角，避免露出尖角
    s.fillRoundRect(BAR_X, yBar, fw, BAR_H, BAR_H / 2, phaseColor());
  }
  String info = String((int)(done / 60000)) + "/" + String((int)(total / 60000)) + " 分钟";
  drawMemoString(s, 312 - memoTextWidth(info), yBar + 20, info, subColor);
}

// 只重绘大数字区(每秒)
static void drawDigits(){
  clk.setColorDepth(16);
  clk.createSprite(320, DIGIT_BOX_H);
  clk.fillSprite(backFillColor);
  drawTimeText(clk, DIGIT_TOP - DIGIT_BOX_Y);
  clk.pushSprite(0, DIGIT_BOX_Y);
  clk.deleteSprite();
}
// 只重绘进度条区
static void drawBarArea(){
  clk.setColorDepth(16);
  clk.createSprite(320, 44);
  clk.fillSprite(backFillColor);
  drawProgress(clk, BAR_Y - 152);
  clk.pushSprite(0, 152);
  clk.deleteSprite();
}
// 顶栏：番茄钟 | 阶段名 | 第N/4个 + WiFi点
static void drawTopBar(TFT_eSprite &s){
  drawMemoString(s, 8, 8, "番茄钟", orangeColor);
  String name = phaseName();
  drawMemoString(s, (320 - memoTextWidth(name)) / 2, 8, name, phaseColor());
  String idx = String("第 ") + String(focusIndex()) + "/" + String(cycleCount()) + " 个";
  drawMemoString(s, 294 - memoTextWidth(idx), 8, idx, subColor);
  s.fillCircle(310, 14, 5, netConnected() ? greenColor : wifiRedColor);
  s.drawFastHLine(0, 28, 320, dividerColor);
}

static void beginPhase(PomoPhase p, bool run){
  phase = p;
  totalMs = (uint32_t)phaseMinutes(p) * 60000UL;
  remainMs = totalMs;
  running = run;
  finishing = false;
  lastMs = millis();
  lastShownSec = -1;
  lastBarMs = 0;
}

// 整页绘制
void drawPomoPage(){
  clk.setColorDepth(16);
  clk.createSprite(320, 240);
  clk.fillSprite(backFillColor);
  if(finishing){
    drawMemoString(clk, (320 - 32) / 2, 104, "完成", orangeColor);
    clk.pushSprite(0, 0);
    clk.deleteSprite();
    return;
  }
  drawTopBar(clk);
  drawTimeText(clk, DIGIT_TOP);
  drawProgress(clk, BAR_Y);
  String tip = "单击 开始/暂停 双击 重置 长按 回日历";
  drawMemoString(clk, (320 - memoTextWidth(tip)) / 2, 202, tip, subColor);
  clk.pushSprite(0, 0);
  clk.deleteSprite();
}

// 当前阶段结束：累计专注数，决定下一阶段，先显示"完成"
static void completePhase(){
  if(phase == POMO_FOCUS){
    focusDone++;
    pendingPhase = (focusDone % cycleCount() == 0) ? POMO_LONG : POMO_SHORT;
  } else {
    pendingPhase = POMO_FOCUS;
  }
  finishing = true;
  running = false;
  remainMs = 0;
  finishUntil = millis() + FINISH_MS;
  if(currentPage == PAGE_POMO) drawPomoPage();
}

void pomoBegin(){
  beginPhase(POMO_FOCUS, false);
}

void pomoLoop(){
  uint32_t now = millis();
  if(finishing){
    if((int32_t)(now - finishUntil) >= 0){
      beginPhase(pendingPhase, true);
      if(currentPage == PAGE_POMO) drawPomoPage();
    }
    return;
  }
  if(running){
    uint32_t dt = now - lastMs;
    lastMs = now;
    if(dt >= remainMs){          // 本阶段走完，自动进入下一阶段
      remainMs = 0;
      completePhase();
      return;
    }
    remainMs -= dt;
  } else {
    lastMs = now;
  }
  if(currentPage != PAGE_POMO) return;
  int sec = (int)((remainMs + 999) / 1000);
  if(sec != lastShownSec){
    lastShownSec = sec;
    drawDigits();
    if(now - lastBarMs > 5000){  // 进度条间隔刷新即可
      lastBarMs = now;
      drawBarArea();
    }
  }
}

// 单击：开始/暂停
void pomoClick(){
  if(finishing) return;          // "完成"期间忽略，稍后自动进下一阶段
  running = !running;
  lastMs = millis();
  if(currentPage == PAGE_POMO) drawPomoPage();
}

// 双击：重置当前阶段(完成提示期间则重置即将开始的阶段)
void pomoReset(){
  beginPhase(finishing ? pendingPhase : phase, false);
  if(currentPage == PAGE_POMO) drawPomoPage();
}

// 网页改了时长：按新时长重置当前阶段
void pomoApplyConfig(){
  beginPhase(phase, false);
  if(currentPage == PAGE_POMO) drawPomoPage();
}
