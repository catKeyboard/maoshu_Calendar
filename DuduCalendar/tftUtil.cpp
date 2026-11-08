#include "tftUtil.h"
#include <time.h>

// 显示基座：全局屏幕对象与主题配色
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite clk = TFT_eSprite(&tft);

// ===== 主题色表 =====
struct ThemeSet {
  uint16_t bg, pen, sub, div, orange, cyan, green, red, lcyan, yellow, sun, sat, date, wred, wait;
};

// ===== 夜间主题(默认)：深蓝黑底 + 亮色文字 =====
static const ThemeSet NIGHT = {
  TFT_BLACK,                    // 背景
  TFT_WHITE,                    // 主文字
  0x8C71,                       // 次要灰
  0x2124,                       // 分隔线
  0xFD20,                       // 橙
  0x5DFF,                       // 青
  0x07E0,                       // 绿
  0xE000,                       // 红
  0xBFFF,                       // 雪(浅青)
  0xFFE0,                       // 雷(黄)
  0xFB60,                       // 周日
  0x6F3F,                       // 周六
  0xD6BA,                       // 平日
  0xF800,                       // 离线红
  0x7BEF                        // 等待灰
};

// ===== 日间主题：浅米底 + 深色文字 =====
static const ThemeSet DAY = {
  0xEF7D,                       // 浅米灰底
  0x2124,                       // 深灰主文字
  0x4228,                       // 次要深灰
  0xB596,                       // 浅灰分隔线
  0xC480,                       // 深橙
  0x045F,                       // 深青
  0x03E0,                       // 深绿
  0xE000,                       // 深红
  0x045F,                       // 雪(深青)
  0xB800,                       // 雷(琥珀)
  0xC480,                       // 周日
  0x045F,                       // 周六
  0x2124,                       // 平日
  0xF800,                       // 离线红
  0x4228                        // 等待灰
};

uint16_t backFillColor, penColor, subColor, dividerColor, orangeColor, cyanColor, greenColor,
         redColor, lightCyanColor, yellowColor, sunColor, satColor, dateColor, wifiRedColor, waitColor;
static bool appliedDay = false;

void applySet(bool day){
  const ThemeSet &t = day ? DAY : NIGHT;
  backFillColor = t.bg;     penColor = t.pen;    subColor = t.sub;
  dividerColor = t.div;     orangeColor = t.orange; cyanColor = t.cyan;
  greenColor = t.green;     redColor = t.red;    lightCyanColor = t.lcyan;
  yellowColor = t.yellow;   sunColor = t.sun;    satColor = t.sat;
  dateColor = t.date;       wifiRedColor = t.wred; waitColor = t.wait;
  appliedDay = day;
}

void tftInit(){
  tft.init();
  tft.setRotation(1);   // 横屏320x240; 若装上后上下颠倒改 3
  tft.setSwapBytes(true);
  applySet(false);      // 启动先按夜间(联网校时后themeTick会自动纠正)
  reflashTFT();
}

void reflashTFT(){
  tft.fillScreen(backFillColor);
}

// 按时段套用主题(7:00-19:00日间)，主题发生变化时返回true
bool themeTick(){
  bool day = false;
  time_t now = time(nullptr);
  if(now > 100000){
    int h = localtime(&now)->tm_hour;
    day = (h >= 7 && h < 19);
  }
  if(day != appliedDay){
    applySet(day);
    reflashTFT();
    return true;
  }
  return false;
}
