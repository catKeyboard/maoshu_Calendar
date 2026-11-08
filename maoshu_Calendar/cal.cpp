#include <time.h>
#include <WiFi.h>
#include "common.h"
#include "tftUtil.h"
#include "memo.h"
#include "font/memo_font_16.h"
#include "weather.h"
#include "net.h"
#include "wallpaper.h"
#include "settings.h"
#include "cal.h"

// ==================== 农历数据(1900-2100) ====================
// 取自 solarlunar 3.1.0(社区持续修正版)：每年一个压缩值
// 低4位=闰月月份(0无闰)，0x10000位=闰月大小，0x8~0x10位=12个月大小(1为大月30天)
PROGMEM const unsigned int lunarInfo[201] = {
0x04bd8,0x04ae0,0x0a570,0x054d5,0x0d260,0x0d950,0x16554,0x056a0,0x09ad0,0x055d2,//1900-1909
0x04ae0,0x0a5b6,0x0a4d0,0x0d250,0x1d255,0x0b540,0x0d6a0,0x0ada2,0x095b0,0x14977,//1910-1919
0x04970,0x0a4b0,0x0b4b5,0x06a50,0x06d40,0x1ab54,0x02b60,0x09570,0x052f2,0x04970,//1920-1929
0x06566,0x0d4a0,0x0ea50,0x06e95,0x05ad0,0x02b60,0x186e3,0x092e0,0x1c8d7,0x0c950,//1930-1939
0x0d4a0,0x1d8a6,0x0b550,0x056a0,0x1a5b4,0x025d0,0x092d0,0x0d2b2,0x0a950,0x0b557,//1940-1949
0x06ca0,0x0b550,0x15355,0x04da0,0x0a5b0,0x14573,0x052b0,0x0a9a8,0x0e950,0x06aa0,//1950-1959
0x0aea6,0x0ab50,0x04b60,0x0aae4,0x0a570,0x05260,0x0f263,0x0d950,0x05b57,0x056a0,//1960-1969
0x096d0,0x04dd5,0x04ad0,0x0a4d0,0x0d4d4,0x0d250,0x0d558,0x0b540,0x0b6a0,0x195a6,//1970-1979
0x095b0,0x049b0,0x0a974,0x0a4b0,0x0b27a,0x06a50,0x06d40,0x0af46,0x0ab60,0x09570,//1980-1989
0x04af5,0x04970,0x064b0,0x074a3,0x0ea50,0x06b58,0x05ac0,0x0ab60,0x096d5,0x092e0,//1990-1999
0x0c960,0x0d954,0x0d4a0,0x0da50,0x07552,0x056a0,0x0abb7,0x025d0,0x092d0,0x0cab5,//2000-2009
0x0a950,0x0b4a0,0x0baa4,0x0ad50,0x055d9,0x04ba0,0x0a5b0,0x15176,0x052b0,0x0a930,//2010-2019
0x07954,0x06aa0,0x0ad50,0x05b52,0x04b60,0x0a6e6,0x0a4e0,0x0d260,0x0ea65,0x0d530,//2020-2029
0x05aa0,0x076a3,0x096d0,0x04afb,0x04ad0,0x0a4d0,0x1d0b6,0x0d250,0x0d520,0x0dd45,//2030-2039
0x0b5a0,0x056d0,0x055b2,0x049b0,0x0a577,0x0a4b0,0x0aa50,0x1b255,0x06d20,0x0ada0,//2040-2049
0x14b63,0x09370,0x049f8,0x04970,0x064b0,0x168a6,0x0ea50,0x06b20,0x1a6c4,0x0aae0,//2050-2059
0x092e0,0x0d2e3,0x0c960,0x0d557,0x0d4a0,0x0da50,0x05d55,0x056a0,0x0a6d0,0x055d4,//2060-2069
0x052d0,0x0a9b8,0x0a950,0x0b4a0,0x0b6a6,0x0ad50,0x055a0,0x0aba4,0x0a5b0,0x052b0,//2070-2079
0x0b273,0x06930,0x07337,0x06aa0,0x0ad50,0x14b55,0x04b60,0x0a570,0x054e4,0x0d160,//2080-2089
0x0e968,0x0d520,0x0daa0,0x16aa6,0x056d0,0x04ae0,0x0a9d4,0x0a4d0,0x0d150,0x0f252,//2090-2099
0x0d520//2100
};

// ==================== 农历基础算法 ====================
static bool isLeapYear(int y){
  return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}
// 公历 y-m-d 距 1900-01-01 的天数
static long daysSince1900(int y, int m, int d){
  static const int md[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  long days = 0;
  for(int yy = 1900; yy < y; yy++) days += isLeapYear(yy) ? 366 : 365;
  for(int mm = 1; mm < m; mm++){
    days += md[mm-1];
    if(mm == 2 && isLeapYear(y)) days++;
  }
  return days + d - 1;
}
static int leapDays(int y);  // 前置声明

// 农历某年总天数
static int lYearDays(int y){
  unsigned int info = pgm_read_dword(&lunarInfo[y - 1900]);
  int sum = 348;
  for(unsigned int i = 0x8000; i > 0x8; i >>= 1) sum += (info & i) ? 1 : 0;
  return sum + leapDays(y);
}
// 闰月月份(0=无闰月)
static int leapMonthOf(int y){
  return pgm_read_dword(&lunarInfo[y - 1900]) & 0xf;
}
// 闰月天数
static int leapDays(int y){
  if(leapMonthOf(y)) return (pgm_read_dword(&lunarInfo[y - 1900]) & 0x10000) ? 30 : 29;
  return 0;
}
// 农历普通月天数
static int lMonthDays(int y, int m){
  return (pgm_read_dword(&lunarInfo[y - 1900]) & (0x10000 >> m)) ? 30 : 29;
}

LunarInfo calSolar2Lunar(int cy, int cm, int cd){
  LunarInfo r = {0};
  // 1900-01-31 为农历1900年正月初一
  long offset = daysSince1900(cy, cm, cd) - 30;
  int i, temp = 0;
  for(i = 1900; i < 2101 && offset > 0; i++){
    temp = lYearDays(i);
    offset -= temp;
  }
  if(offset < 0){ offset += temp; i--; }
  r.lYear = i;
  // 逐月推算月/日
  int leap = leapMonthOf(i);
  bool isLeap = false;
  int j;
  for(j = 1; j < 13 && offset > 0; j++){
    if(leap > 0 && j == leap + 1 && !isLeap){
      --j;
      isLeap = true;
      temp = leapDays(i);
    } else {
      temp = lMonthDays(i, j);
    }
    if(isLeap && j == leap + 1) isLeap = false;  // 解除闰月状态
    offset -= temp;
  }
  if(offset == 0 && leap > 0 && j == leap + 1){
    if(isLeap) isLeap = false;
    else { isLeap = true; --j; }
  }
  if(offset < 0){ offset += temp; --j; }
  r.lMonth = j;
  r.lDay = (int)offset + 1;
  r.isLeap = isLeap;
  // 农历月名
  static const char* mn[12] = {"正","二","三","四","五","六","七","八","九","十","冬","腊"};
  r.monthCn = String(isLeap ? "闰" : "") + mn[(j - 1) % 12] + "月";
  // 农历日名
  static const char* nn[10] = {"一","二","三","四","五","六","七","八","九","十"};
  if(r.lDay <= 10)      r.dayCn = String("初") + nn[r.lDay - 1];
  else if(r.lDay < 20)  r.dayCn = String("十") + nn[r.lDay - 11];
  else if(r.lDay == 20) r.dayCn = "二十";
  else if(r.lDay < 30)  r.dayCn = String("廿") + nn[r.lDay - 21];
  else                  r.dayCn = "三十";
  return r;
}

void calInit(){
  // 预留
}

// ==================== 桌面页(原日历页) ====================
// 大号ASCII字符(8x16点阵放大sc倍)
static void drawBigChar(TFT_eSprite &s, int x, int y, char c, int sc, uint16_t color){
  if(c < 0x20 || c > 0x7E) return;
  for(int row = 0; row < 16; row++){
    unsigned char bits = pgm_read_byte(&memoFontAscii[c - 0x20][row]);
    for(int col = 0; col < 8; col++){
      if(bits & (0x80 >> col)) s.fillRect(x + col * sc, y + row * sc, sc, sc, color);
    }
  }
}
// 日干支序号(0=甲子)：以1949-10-01为甲子日校准
static int calDayGZ(int y, int m, int d){
  int a = (14 - m) / 12, yy = y + 4800 - a, mm = m + 12 * a - 3;
  long jdn = d + (153 * mm + 2) / 5 + 365L * yy + yy / 4 - yy / 100 + yy / 400 - 32045;
  return (int)((jdn + 49) % 60);
}
// 建除十二神宜忌(简化通书版)：每日 4 宜 4 忌，面板内分 2 行显示
static const char* YIJI[12][8] = {
  // 宜1    宜2    宜3    宜4    忌1    忌2    忌3    忌4
  {"出行","开业","签约","纳财", "动土","安葬","词讼","搬迁"},  // 建
  {"打扫","求医","拆屋","祭祀", "嫁娶","出行","签约","开市"},  // 除
  {"祭祀","纳财","安床","签约", "求医","搬迁","开市","词讼"},  // 满
  {"修造","打扫","签约","纳财", "出行","动土","登高","安葬"},  // 平
  {"嫁娶","签约","安床","祭祀", "词讼","搬迁","出行","拆屋"},  // 定
  {"捕捉","修造","纳财","开市", "出行","搬迁","词讼","安葬"},  // 执
  {"拆屋","求医","打扫","修坟", "嫁娶","开市","签约","出行"},  // 破
  {"安床","祭祀","求医","纳财", "登高","出行","动土","开市"},  // 危
  {"开业","嫁娶","出行","签约", "词讼","安葬","拆屋","动土"},  // 成
  {"纳财","签约","捕捉","修造", "开市","安葬","搬迁","词讼"},  // 收
  {"开工","开业","修造","纳财", "安葬","词讼","拆屋","出行"},  // 开
  {"修坟","安葬","纳财","祭祀", "开市","出行","求医","嫁娶"}   // 闭
};

// 黄历：返回宜/忌各4项(面板内按两行两列对齐显示)
static void calYiJi(int y, int m, int d, LunarInfo& li, String yi[4], String ji[4]){
  int dayBr = calDayGZ(y, m, d) % 12;              // 日支(子=0)
  int monBr = (li.lMonth + 1) % 12;                // 月支(正月寅=2)
  int shen = (dayBr - monBr + 12) % 12;
  for(int i = 0; i < 4; i++){
    yi[i] = YIJI[shen][i];
    ji[i] = YIJI[shen][4 + i];
  }
}

// ==================== 桌面页(原日历页) ====================
// 布局：参考插画右侧的深色面板——星期行 / 大时钟 / 农历 / 天气 / 宜
// 交互：单击→备忘录，双击→天气页，三击→壁纸页
#define PANEL_X 204     // 面板区域(与第1套壁纸自带面板对齐)
#define PANEL_Y 6
#define PANEL_W 104
#define PANEL_H 222
#define PANEL_BG  0x1907   // 深蓝面板底色
#define GOLD      0xE64F   // 主金色
#define GOLD_DIM  0xB50F   // 次金色

static inline uint16_t swapColor(uint16_t c){ return (uint16_t)((c >> 8) | (c << 8)); }
// 两色按 a/255 向 bg 混合
static inline uint16_t mixColor(uint16_t c, uint16_t bg, uint8_t a){
  uint16_t ia = 255 - a;
  uint16_t r = ((((c >> 11) & 0x1F) * ia) + (((bg >> 11) & 0x1F) * a)) / 255;
  uint16_t g = ((((c >>  5) & 0x3F) * ia) + (((bg >>  5) & 0x3F) * a)) / 255;
  uint16_t b = ((((c      ) & 0x1F) * ia) + (((bg      ) & 0x1F) * a)) / 255;
  return (uint16_t)((r << 11) | (g << 5) | b);
}
// 半透明圆角卡片：直接混合精灵像素，圆角外跳过
static void blendCard(TFT_eSprite &s, int x, int y, int w, int h, int r, uint16_t color, uint8_t a){
  uint16_t *buf = (uint16_t*)s.getPointer();
  if(buf == nullptr) return;
  for(int yy = 0; yy < h; yy++){
    int dy = 0;
    if(yy < r) dy = r - yy;
    else if(yy >= h - r) dy = yy - (h - r) + 1;
    for(int xx = 0; xx < w; xx++){
      int dx = 0;
      if(xx < r) dx = r - xx;
      else if(xx >= w - r) dx = xx - (w - r) + 1;
      if(dx && dy && dx * dx + dy * dy > r * r) continue;
      int idx = (y + yy) * 320 + (x + xx);
      buf[idx] = swapColor(mixColor(swapColor(buf[idx]), color, a));
    }
  }
}

void drawCalPage(){
  time_t now = time(nullptr);
  clk.setColorDepth(16);
  clk.createSprite(320, 240);
  bool wp = wallpaperOn();
  if(wp) wallpaperDrawBg(clk);   // 壁纸原图铺底(文字都在深色面板内)
  else clk.fillSprite(backFillColor);
  // 右侧面板：有壁纸时半透明压深(保留插画装饰)，无壁纸时接近实底
  blendCard(clk, PANEL_X, PANEL_Y, PANEL_W, PANEL_H, 10, PANEL_BG, wp ? 170 : 235);
  if(now < 100000){  // NTP尚未同步
    drawMemoString(clk, 112, 100, "等待时间同步", waitColor);
    drawMemoString(clk, 12, 150, netDiag(), cyanColor);
    clk.pushSprite(0, 0);
    clk.deleteSprite();
    return;
  }
  struct tm* t = localtime(&now);
  int y = t->tm_year + 1900, m = t->tm_mon + 1, d = t->tm_mday;
  static const char* wk[7] = {"日","一","二","三","四","五","六"};
  const int PCX = PANEL_X + PANEL_W / 2;   // 面板水平中心

  // ---- 星期(仅当天) ----
  String wkd = String("星期") + wk[t->tm_wday];
  drawMemoString(clk, PCX - memoTextWidth(wkd) / 2, 22, wkd, GOLD);
  // ---- 大时钟(8x16 ASCII 放大2倍) ----
  char hm[6];
  snprintf(hm, sizeof(hm), "%02d:%02d", t->tm_hour, t->tm_min);
  for(int i = 0; i < 5; i++) drawBigChar(clk, PCX - 40 + i * 16, 46, hm[i], 2, GOLD);
  // ---- 农历 ----
  LunarInfo li = calSolar2Lunar(y, m, d);
  String ld = li.monthCn + li.dayCn;
  drawMemoString(clk, PCX - memoTextWidth(ld) / 2, 80, ld, GOLD_DIM);
  // ---- 今日天气(天气词 + 最高温) ----
  String wline = "天气获取中";
  uint16_t wcol = GOLD_DIM;
  if(weatherValid()){
    static const char* WTEXT[7] = {"晴","多云","阴","雨","雪","雾","雷雨"};
    wline = WTEXT[weatherCode(0)];
    if(weatherHasTemp()) wline += String(" ") + String(weatherHigh(0)) + "℃";
    wcol = GOLD;
  }
  drawMemoString(clk, PCX - memoTextWidth(wline) / 2, 102, wline, wcol);
  // ---- 今日宜 / 忌：两行两列对齐 ----
  String yi[4], ji[4];
  calYiJi(y, m, d, li, yi, ji);
  const int COL1 = 228, COL2 = 272;   // 两列起点(中文16px宽)
  drawMemoString(clk, 206, 124, "宜", GOLD);
  drawMemoString(clk, COL1, 124, yi[0], GOLD);
  drawMemoString(clk, COL2, 124, yi[1], GOLD);
  drawMemoString(clk, COL1, 142, yi[2], GOLD);
  drawMemoString(clk, COL2, 142, yi[3], GOLD);
  drawMemoString(clk, 206, 160, "忌", GOLD_DIM);
  drawMemoString(clk, COL1, 160, ji[0], GOLD_DIM);
  drawMemoString(clk, COL2, 160, ji[1], GOLD_DIM);
  drawMemoString(clk, COL1, 178, ji[2], GOLD_DIM);
  drawMemoString(clk, COL2, 178, ji[3], GOLD_DIM);

  // WiFi状态点
  clk.fillCircle(310, 6, 4, netConnected() ? greenColor : wifiRedColor);
  clk.pushSprite(0, 0);
  clk.deleteSprite();
}

// ==================== 天气页(7天预报) ====================
void drawWeatherPage(){
  clk.setColorDepth(16);
  clk.createSprite(320, 240);
  clk.fillSprite(backFillColor);
  drawMemoString(clk, 8, 8, "本周天气", orangeColor);
  String city = cfgWeatherCity();
  if(city.length()) drawMemoString(clk, 150 - memoTextWidth(city) / 2, 8, city, penColor);
  clk.fillCircle(310, 14, 5, netConnected() ? greenColor : wifiRedColor);
  clk.drawFastHLine(0, 28, 320, dividerColor);
  if(!weatherValid()){
    String s = "天气获取中";
    drawMemoString(clk, (320 - memoTextWidth(s)) / 2, 108, s, subColor);
  } else {
    static const char* WTEXT[7] = {"晴","多云","阴","雨","雪","雾","雷雨"};
    const uint16_t WCOL[7] = {orangeColor, penColor, subColor, cyanColor,
                              lightCyanColor, subColor, yellowColor};
    static const char* wk[7] = {"日","一","二","三","四","五","六"};
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    for(int i = 0; i < 7; i++){
      int ry = 32 + i * 27;
      time_t dayT = now + (time_t)i * 86400;
      struct tm* dt = localtime(&dayT);
      String label = (i == 0) ? "今天" : String("周") + wk[dt->tm_wday];
      drawMemoString(clk, 12, ry, label, i == 0 ? orangeColor : penColor);
      char md[10];
      snprintf(md, sizeof(md), "%d/%d", dt->tm_mon + 1, dt->tm_mday);
      drawMemoString(clk, 72, ry, md, subColor);
      int wc = weatherCode(i);
      drawMemoString(clk, 146, ry, WTEXT[wc], WCOL[wc]);
      String temp = "--";
      if(weatherHasTemp()) temp = String(weatherHigh(i)) + "/" + String(weatherLow(i)) + "℃";
      drawMemoString(clk, 312 - memoTextWidth(temp), ry, temp, penColor);
      if(i < 6) clk.drawFastHLine(12, ry + 24, 296, dividerColor);
    }
  }
  String tip = "单击 返回桌面";
  drawMemoString(clk, (320 - memoTextWidth(tip)) / 2, 218, tip, subColor);
  clk.pushSprite(0, 0);
  clk.deleteSprite();
}
