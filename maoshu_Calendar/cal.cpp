#include <time.h>
#include <WiFi.h>
#include "common.h"
#include "tftUtil.h"
#include "memo.h"
#include "font/memo_font_16.h"
#include "weather.h"
#include "net.h"
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

// 公历节日(按月日匹配)
static String solarFestivalName(int m, int d){
  switch(m * 100 + d){
    case 101:  return "元旦";
    case 214:  return "情人节";
    case 308:  return "妇女节";
    case 312:  return "植树节";
    case 501:  return "劳动节";
    case 504:  return "青年节";
    case 601:  return "儿童节";
    case 701:  return "建党节";
    case 801:  return "建军节";
    case 910:  return "教师节";
    case 1001: return "国庆节";
    case 1225: return "圣诞节";
  }
  return "";
}
// 农历节日(除夕=腊月最后一天，1900-2100年间无闰腊月)
static String lunarFestivalName(const LunarInfo& li){
  if(li.isLeap) return "";
  if(li.lMonth == 1 && li.lDay == 1)  return "春节";
  if(li.lMonth == 1 && li.lDay == 15) return "元宵节";
  if(li.lMonth == 5 && li.lDay == 5)  return "端午节";
  if(li.lMonth == 7 && li.lDay == 7)  return "七夕节";
  if(li.lMonth == 8 && li.lDay == 15) return "中秋节";
  if(li.lMonth == 9 && li.lDay == 9)  return "重阳节";
  if(li.lMonth == 12 && li.lDay == 8) return "腊八节";
  if(li.lMonth == 12 && li.lDay == lMonthDays(li.lYear, 12)) return "除夕";
  return "";
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
  // 节日：农历优先，其次公历
  r.festival = lunarFestivalName(r);
  if(!r.festival.length()) r.festival = solarFestivalName(cm, cd);
  return r;
}

String calYearGZ(int lYear){
  static const char* gan[10] = {"甲","乙","丙","丁","戊","己","庚","辛","壬","癸"};
  static const char* zhi[12] = {"子","丑","寅","卯","辰","巳","午","未","申","酉","戌","亥"};
  static const char* ani[12] = {"鼠","牛","虎","兔","龙","蛇","马","羊","猴","鸡","狗","猪"};
  int g = (lYear - 4) % 10;
  int z = (lYear - 4) % 12;
  return String(gan[g]) + zhi[z] + ani[z] + "年";
}

int calDaysInMonth(int y, int m){
  static const int md[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if(m == 2) return isLeapYear(y) ? 29 : 28;
  return md[m - 1];
}

int calDow(int y, int m, int d){
  // 1900-01-01 为周一，0=周日
  return (int)((daysSince1900(y, m, d) + 1) % 7);
}

void calInit(){
  // 预留
}

// ==================== 日历页绘制(横屏320x240, 按用户示意图) ====================
// 布局：顶栏(日期+星期+WiFi点) | 左:月历(y28..194)+天气条(y196..240) | 右:大时钟/黄历/宜/忌
// 单元格内绘制数字(ASCII 8x16 在 cw 宽度内居中)
static void drawNum(TFT_eSprite &s, int gx, int cw, int y, int n, uint16_t c){
  String t = String(n);
  drawMemoString(s, gx + (cw - (int)t.length() * 8) / 2, y, t, c);
}
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
// 建除十二神宜忌(简化通书版)
static const char* YIJI[12][2] = {
  {"出行","动土"},{"打扫","嫁娶"},{"祭祀","求医"},{"修造","词讼"},
  {"签约","出行"},{"捕捉","搬迁"},{"拆屋","开市"},{"安床","登高"},
  {"开业","诉讼"},{"纳财","安葬"},{"开工","安葬"},{"修坟","开市"}
};
static const char* SHEN12[12] = {"建","除","满","平","定","执","破","危","成","收","开","闭"};

// 右栏黄历：返回宜/忌文本
static void calYiJi(int y, int m, int d, LunarInfo& li, String& yi, String& ji){
  int dayBr = calDayGZ(y, m, d) % 12;              // 日支(子=0)
  int monBr = (li.lMonth + 1) % 12;                // 月支(正月寅=2)
  int shen = (dayBr - monBr + 12) % 12;
  yi = String("宜：") + YIJI[shen][0];
  ji = String("忌：") + YIJI[shen][1];
}

void drawCalPage(){
  time_t now = time(nullptr);
  clk.setColorDepth(16);
  clk.createSprite(320, 240);
  clk.fillSprite(backFillColor);
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
  // ---- 顶栏：2026年11月3日(橙) + 星期二(主文字色) + WiFi状态点 ----
  char title[24];
  snprintf(title, sizeof(title), "%d年%d月%d日", y, m, d);
  drawMemoString(clk, 8, 8, String(title), orangeColor);
  drawMemoString(clk, 124, 8, String("星期") + wk[t->tm_wday], penColor);
  if(netConnected()) drawMemoString(clk, 180, 8, WiFi.localIP().toString(), subColor);  // IP即配置页地址
  clk.fillCircle(310, 14, 5, netConnected() ? greenColor : wifiRedColor);
  clk.drawFastHLine(0, 28, 320, dividerColor);
  // ---- 左侧月历：7列x27px，起点x=2，y28..194 ----
  for(int i = 0; i < 7; i++){
    uint16_t c = (i == 0) ? orangeColor : (i == 6 ? cyanColor : subColor);
    drawMemoString(clk, 2 + i * 27 + 5, 34, wk[i], c);
  }
  int col = calDow(y, m, 1), row = 0;
  int dim = calDaysInMonth(y, m);
  for(int dd = 1; dd <= dim; dd++){
    int gx = 2 + col * 27, gy = 56 + row * 22;
    if(dd == d){  // 今日高亮
      clk.fillRoundRect(gx + 1, gy - 4, 25, 20, 5, redColor);
      drawNum(clk, gx, 27, gy, dd, TFT_WHITE);
    } else {
      uint16_t c = (col == 0) ? sunColor : (col == 6 ? satColor : dateColor);
      drawNum(clk, gx, 27, gy, dd, c);
    }
    if(++col > 6){ col = 0; row++; }
  }
  // ---- 左下天气条：7天，标签+天气字 ----
  clk.drawFastHLine(0, 196, 196, dividerColor);
  if(weatherValid()){
    static const char* WTEXT[7] = {"晴","云","阴","雨","雪","雾","雷"};
    const uint16_t WCOL[7] = {orangeColor, penColor, subColor, cyanColor, lightCyanColor, subColor, yellowColor};
    for(int i = 0; i < 7; i++){
      int wx = 2 + i * 27 + 5;
      int wd = (t->tm_wday + i) % 7;
      drawMemoString(clk, wx, 202, String(i == 0 ? "今" : wk[wd]), subColor);
      drawMemoString(clk, wx, 222, WTEXT[weatherCode(i)], WCOL[weatherCode(i)]);
    }
  } else {
    drawMemoString(clk, 54, 210, "天气获取中", subColor);
  }
  // ---- 右栏(x196..320)：大时钟/黄历/宜/忌 ----
  clk.drawFastVLine(196, 28, 212, dividerColor);
  char hm[6];
  snprintf(hm, sizeof(hm), "%02d:%02d", t->tm_hour, t->tm_min);
  for(int i = 0; i < 5; i++) drawBigChar(clk, 198 + i * 24, 38, hm[i], 3, penColor);
  clk.drawFastHLine(196, 96, 124, dividerColor);
  LunarInfo li = calSolar2Lunar(y, m, d);
  drawMemoString(clk, 226, 100, calYearGZ(li.lYear), greenColor);      // 丙午马年
  drawMemoString(clk, 226, 120, li.monthCn + li.dayCn, greenColor);    // 九月初四
  clk.drawFastHLine(196, 144, 124, dividerColor);
  String yi, ji;
  calYiJi(y, m, d, li, yi, ji);
  drawMemoString(clk, 226, 160, yi, greenColor);                       // 宜：xx(绿)
  clk.drawFastHLine(196, 192, 124, dividerColor);
  drawMemoString(clk, 226, 208, ji, redColor);                         // 忌：xx(红)
  clk.pushSprite(0, 0);
  clk.deleteSprite();
}
